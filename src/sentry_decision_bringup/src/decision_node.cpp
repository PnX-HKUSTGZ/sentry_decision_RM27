#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_bringup/config_loader.hpp"
#include "sentry_decision_bringup/runtime_config.hpp"
#include "sentry_decision_bringup/tree_loader.hpp"
#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/nav_goal_tracker.hpp"
#include "sentry_decision_core/safety_supervisor.hpp"
#include "sentry_decision_core/tactical_override.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_io/decision_state_publisher.hpp"
#include "sentry_decision_io/panel_service.hpp"
#include "sentry_decision_io/ros_io_node.hpp"
#include "sentry_decision_nodes/nodes.hpp"
#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"
#include "sentry_decision_viz/groot2_bridge.hpp"
#include "sentry_decision_viz/tree_state_publisher.hpp"

#ifndef DEFAULT_TREE_PATH
#define DEFAULT_TREE_PATH "tree/root.xml"
#endif
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "config/profiles.yaml"
#endif
#ifndef DEFAULT_MODULE_LIB_DIR
#define DEFAULT_MODULE_LIB_DIR ""
#endif

namespace {

using sentry_decision::Duration;
using sentry_decision::SteadyClock;
using sentry_decision::TimePoint;

struct Options {
  double rate_hz = 20.0;
  int ticks = 0;  // 0 表示一直运行
  std::string tree = DEFAULT_TREE_PATH;
  std::string config = DEFAULT_CONFIG_PATH;
  std::string plugin;
  int groot2_port = 0;  // >0 时开启 Groot2 桥
};

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "参数 " << name << " 缺少取值\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "--rate") {
      options.rate_hz = std::stod(next("--rate"));
    } else if (arg == "--ticks") {
      options.ticks = std::stoi(next("--ticks"));
    } else if (arg == "--tree") {
      options.tree = next("--tree");
    } else if (arg == "--config") {
      options.config = next("--config");
    } else if (arg == "--plugin") {
      options.plugin = next("--plugin");
    } else if (arg == "--groot2-port") {
      options.groot2_port = std::stoi(next("--groot2-port"));
    } else if (arg == "--ros-args") {
      // 其余 ROS 参数（节点参数 / 重映射）留给 rclcpp，与 referee_sim_node 一致。
      break;
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      std::exit(2);
    }
  }
  if (!(options.rate_hz > 0.0) || options.rate_hz > 1000.0) {
    std::cerr << "参数 --rate 必须在 (0, 1000] Hz 之间\n";
    std::exit(2);
  }
  if (options.groot2_port < 0 || options.groot2_port > 65535) {
    std::cerr << "参数 --groot2-port 必须在 [0, 65535] 之间（0 表示关闭）\n";
    std::exit(2);
  }
  return options;
}

std::string join_errors(const std::vector<std::string>& errors) {
  std::string text;
  for (const auto& error : errors) {
    if (!text.empty()) {
      text += "; ";
    }
    text += error;
  }
  return text;
}

const char* action_kind_name(sentry_decision::DecisionActionKind kind) {
  using K = sentry_decision::DecisionActionKind;
  switch (kind) {
    case K::kAmmoExchange:
      return "ammo_exchange";
    case K::kHpExchange:
      return "hp_exchange";
    case K::kFreeResurrect:
      return "free_resurrect";
    case K::kInstantResurrect:
      return "instant_resurrect";
    case K::kRemoteAmmoExchange:
      return "remote_ammo_exchange";
    case K::kRemoteHpExchange:
      return "remote_hp_exchange";
    case K::kNone:
    default:
      return "none";
  }
}

// 最小 JSON 字符串转义，用于把回执 detail 放进 list_state。
std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// 真实 IO 决策节点：RosIoNode 提供信念输入与执行端，本节点跑行为树与仲裁，
// 并把仲裁结果下发（导航目标 / 速度 / 决策动作）与发布决策状态。
class DecisionNode : public rclcpp::Node {
 public:
  DecisionNode(std::shared_ptr<sentry_decision_io::RosIoNode> io, const Options& options,
               const sentry_decision::PolicyConfig* config)
      : Node("sentry_decision"),
        io_(std::move(io)),
        world_model_(*io_, *io_, *io_, sentry_decision_bringup::timeouts_from_config(config)),
        dispatcher_(sentry_decision_bringup::action_config_from_config(config)),
        safety_(sentry_decision_bringup::safety_limits_from_config(config)),
        state_publisher_(*this),
        max_ticks_(options.ticks) {
    context_.config = config;
    if (config != nullptr) {
      policy_ = sentry_decision::RuleBasedStrategicPolicy::from_config(*config);
    }
    build_tree(options.tree, options.plugin);
    tree_publisher_.emplace(*this, *tree_);
    if (options.groot2_port > 0) {
      groot2_ = std::make_unique<sentry_decision_viz::Groot2Bridge>(
          *tree_, static_cast<unsigned>(options.groot2_port));
      SD_LOG_ACT("viz", "Groot2 已监听端口 %d", options.groot2_port);
    }
    panel_service_ = std::make_shared<sentry_decision_io::PanelService>(
        *this, "/decision/debug", [this]() { return state_snapshot_json(); });
    const auto period =
        std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / options.rate_hz));
    timer_ = create_wall_timer(period, [this]() { tick(); });
  }

 private:
  void build_tree(const std::string& tree_path, const std::string& plugin) {
    std::vector<std::string> errors;
    if (!plugin.empty()) {
      factory_.registerFromPlugin(plugin);
    }
    sentry_decision_bringup::TreeSetupOptions tree_options;
    tree_options.module_lib_dir = DEFAULT_MODULE_LIB_DIR;
    tree_options.load_modules = plugin.empty();
    tree_options.register_builtin = plugin.empty();
    if (!sentry_decision_bringup::setup_tree_factory(factory_, tree_path, context_.config, &errors,
                                                     tree_options)) {
      throw std::runtime_error("行为树校验失败: " + join_errors(errors));
    }
    auto blackboard = BT::Blackboard::create();
    blackboard->set("context", &context_);
    tree_ = std::make_unique<BT::Tree>(factory_.createTreeFromFile(tree_path, blackboard));
  }

  void tick() {
    const TimePoint now = SteadyClock::now();
    // 先应用上一节拍入队的战术层覆盖，再取世界快照并求值战略层。
    apply_tactical_commands(now);
    context_.world = world_model_.snapshot(now);
    context_.clear_intents();
    context_.apply_strategy(policy_.decide(context_.world));
    sentry_decision::apply_tactical_override(tactical_override_, now, &context_);
    // 清空上一拍的树状态缓存，让快照只反映本拍执行的节点。
    tree_publisher_->begin_tick();
    const TimePoint tick_begin = SteadyClock::now();
    tree_->tickOnce();
    const double tick_ms =
        std::chrono::duration<double, std::milli>(SteadyClock::now() - tick_begin).count();

    // 每 tick 重建来源：清掉上一拍的树内意图，避免残留。
    arbiter_.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter_.clear_source(sentry_decision::SourceId::kSkill);
    for (const auto& intent : context_.intents) {
      arbiter_.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter_.resolve(now);

    // 安全监督：仲裁后做最终限幅与急停兜底。
    const sentry_decision::SafetyResult safe = safety_.apply(context_.world, result.output);
    if (safe.emergency != safety_emergency_) {
      if (safe.emergency) {
        SD_LOG_WARN("safety", "触发急停: %s", join_errors(safe.reasons).c_str());
      } else {
        SD_LOG_ACT("safety", "急停解除");
      }
      safety_emergency_ = safe.emergency;
    }
    sentry_decision::ArbiterResult safe_result = result;
    safe_result.output = safe.output;

    apply_nav(safe_result);
    if (safe_result.output.cmd_vel.has_value()) {
      io_->set_velocity(*safe_result.output.cmd_vel);
    }
    apply_actions(safe_result, now);

    const std::uint32_t tick = tick_count_++;
    state_publisher_.publish(context_.world, safe_result, tick);
    tree_publisher_->publish(tick, tick_ms);
    update_state_snapshot(safe_result, now);

    if (max_ticks_ > 0 && tick_count_ >= static_cast<std::uint32_t>(max_ticks_)) {
      rclcpp::shutdown();
    }
  }

  // 面板：tick 边界应用本拍入队的战术层覆盖命令。
  void apply_tactical_commands(TimePoint now) {
    if (!panel_service_) {
      return;
    }
    for (const auto& command : panel_service_->take_commands()) {
      using Kind = sentry_decision_io::TacticalOverrideCommand::Kind;
      if (command.kind == Kind::kSet) {
        tactical_override_.set(command.mode, command.lease, now);
        SD_LOG_ACT("panel", "战术层覆盖 mode=%d lease=%.1fs", static_cast<int>(command.mode),
                   static_cast<double>(command.lease.count()) / 1000.0);
      } else {
        tactical_override_.clear();
        SD_LOG_ACT("panel", "清除战术层覆盖");
      }
    }
  }

  // 刷新 list_state 的 JSON 快照（面板轮询读取）。
  void update_state_snapshot(const sentry_decision::ArbiterResult& result, TimePoint now) {
    std::ostringstream out;
    out << "{\"points\":[";
    bool first = true;
    if (context_.config != nullptr) {
      for (const auto& entry : context_.config->points) {
        if (!first) {
          out << ",";
        }
        first = false;
        // 可选半径：仅供面板把增益点画成虚线环（0 = 普通点）。
        double radius = 0.0;
        const auto radius_it = context_.config->point_radius.find(entry.first);
        if (radius_it != context_.config->point_radius.end()) {
          radius = radius_it->second;
        }
        out << "{\"name\":\"" << json_escape(entry.first) << "\",\"x\":" << entry.second.x
            << ",\"y\":" << entry.second.y << ",\"r\":" << radius << "}";
      }
    }
    out << "],\"resource\":{\"ammo\":" << result.output.resource.ammo
        << ",\"hp\":" << result.output.resource.hp
        << ",\"remote_ammo\":" << result.output.resource.remote_ammo
        << ",\"remote_hp\":" << result.output.resource.remote_hp
        << ",\"revive\":" << (result.output.resource.revive ? "true" : "false")
        << ",\"instant_revive\":" << (result.output.resource.instant_revive ? "true" : "false")
        << "}";
    out << ",\"last_action\":";
    if (has_last_action_) {
      out << "{\"kind\":\"" << action_kind_name(last_action_.kind)
          << "\",\"value\":" << last_action_.value << ",\"request_id\":" << last_action_.request_id
          << "}";
    } else {
      out << "null";
    }
    out << ",\"last_ack\":";
    if (has_last_ack_) {
      out << "{\"request_id\":" << last_ack_.request_id
          << ",\"accepted\":" << (last_ack_.accepted ? "true" : "false")
          << ",\"code\":" << static_cast<int>(last_ack_.code) << ",\"detail\":\""
          << json_escape(last_ack_.detail) << "\"}";
    } else {
      out << "null";
    }
    out << ",\"safety_emergency\":" << (safety_emergency_ ? "true" : "false");
    out << ",\"tactical_override\":";
    const std::optional<sentry_decision::TacticalMode> override_mode = tactical_override_.mode(now);
    if (override_mode.has_value()) {
      out << "{\"mode\":" << static_cast<int>(*override_mode) << "}";
    } else {
      out << "null";
    }
    out << "}";

    std::lock_guard<std::mutex> lock(state_mutex_);
    state_json_ = out.str();
  }

  std::string state_snapshot_json() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_json_;
  }

  // 导航目标跟随：用 NavGoalTracker 做边沿 / 取消契约，避免每 tick 重发。
  void apply_nav(const sentry_decision::ArbiterResult& result) {
    const sentry_decision::NavGoalTracker::Step step = nav_tracker_.update(result.output.nav_goal);
    if (step.decision == sentry_decision::NavGoalTracker::Decision::kSend &&
        step.goal.has_value()) {
      io_->send_goal(*step.goal);
    } else if (step.decision == sentry_decision::NavGoalTracker::Decision::kCancel) {
      io_->cancel_goal();
    }
  }

  // 资源请求 -> 决策动作：交给派发器处理 one-shot / polled 与 ack，再下发。
  void apply_actions(const sentry_decision::ArbiterResult& result, sentry_decision::TimePoint now) {
    for (const auto& ack : io_->take_acks()) {
      dispatcher_.on_ack(ack);
      last_ack_ = ack;
      has_last_ack_ = true;
    }
    sentry_decision::submit_resource_requests(dispatcher_, result.output.resource);
    for (const auto& action : dispatcher_.poll(now)) {
      io_->send_action(action);
      last_action_ = action;
      has_last_action_ = true;
      SD_LOG_ACT("action", "下发 %s value=%d request_id=%u", action_kind_name(action.kind),
                 action.value, action.request_id);
    }
  }

  std::shared_ptr<sentry_decision_io::RosIoNode> io_;
  sentry_decision::DecisionContext context_;
  sentry_decision::WorldModel world_model_;
  sentry_decision::IntentArbiter arbiter_;
  sentry_decision::ActionDispatcher dispatcher_;
  sentry_decision::TacticalOverride tactical_override_;
  sentry_decision::SafetySupervisor safety_;
  sentry_decision::RuleBasedStrategicPolicy policy_;
  sentry_decision_io::DecisionStatePublisher state_publisher_;
  BT::BehaviorTreeFactory factory_;
  std::unique_ptr<BT::Tree> tree_;
  sentry_decision::NavGoalTracker nav_tracker_;
  bool has_last_action_ = false;
  sentry_decision::DecisionAction last_action_;
  bool has_last_ack_ = false;
  sentry_decision::ActionAck last_ack_;
  bool safety_emergency_ = false;
  std::uint32_t tick_count_ = 0;
  int max_ticks_ = 0;
  std::optional<sentry_decision_viz::TreeStatePublisher> tree_publisher_;
  std::unique_ptr<sentry_decision_viz::Groot2Bridge> groot2_;
  std::shared_ptr<sentry_decision_io::PanelService> panel_service_;
  mutable std::mutex state_mutex_;
  std::string state_json_ = "{}";
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

// 真实 IO 决策入口：io_node 的所有 IO 端口与行为树、仲裁在同一进程内闭环。
int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  const Options options = parse_options(argc, argv);

  auto console = std::make_shared<sentry_decision::ConsoleSink>(false);
  auto& logger = sentry_decision::Logger::instance();
  logger.clear_sinks();
  logger.add_short_sink(console);
  logger.set_short_min_level(sentry_decision::LogLevel::kAct);

  const sentry_decision_bringup::ConfigLoadResult loaded =
      sentry_decision_bringup::load_policy_config(options.config);
  if (!loaded.ok()) {
    std::cerr << "配置加载失败: " << options.config << "\n";
    for (const auto& error : loaded.errors) {
      std::cerr << "  - " << error << "\n";
    }
    rclcpp::shutdown();
    return 1;
  }
  SD_LOG_ACT("config", "%s", sentry_decision_bringup::format_config(loaded.config).c_str());

  rclcpp::NodeOptions io_options;
  io_options.append_parameter_override("map_frame", loaded.config.map_frame);
  auto io = std::make_shared<sentry_decision_io::RosIoNode>(io_options);
  try {
    auto decision = std::make_shared<DecisionNode>(io, options, &loaded.config);
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(io);
    executor.add_node(decision);
    executor.spin();
  } catch (const std::exception& ex) {
    std::cerr << "启动失败: " << ex.what() << "\n";
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
