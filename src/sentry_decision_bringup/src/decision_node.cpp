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
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_bringup/config_loader.hpp"
#include "sentry_decision_bringup/tree_loader.hpp"
#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/intervention.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/nav_goal_tracker.hpp"
#include "sentry_decision_core/safety_supervisor.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_io/decision_state_publisher.hpp"
#include "sentry_decision_io/intervention_convert.hpp"
#include "sentry_decision_io/intervention_server.hpp"
#include "sentry_decision_io/ros_io_node.hpp"
#include "sentry_decision_msgs/msg/intervention_event.hpp"
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

// 真实 IO 决策节点：RosIoNode 提供信念输入与执行端，本节点跑行为树与仲裁，
// 并把仲裁结果下发（导航目标 / 速度 / 决策动作）与发布决策状态。
class DecisionNode : public rclcpp::Node {
 public:
  DecisionNode(std::shared_ptr<sentry_decision_io::RosIoNode> io, const Options& options,
               const sentry_decision::PolicyConfig* config)
      : Node("sentry_decision"),
        io_(std::move(io)),
        world_model_(*io_, *io_, *io_),
        state_publisher_(*this),
        max_ticks_(options.ticks) {
    context_.config = config;
    if (config != nullptr) {
      policy_ = sentry_decision::RuleBasedStrategicPolicy::from_config(*config);
    }
    build_tree(options.tree, options.plugin);
    tree_publisher_.emplace(*this);
    if (options.groot2_port > 0) {
      groot2_ = std::make_unique<sentry_decision_viz::Groot2Bridge>(
          *tree_, static_cast<unsigned>(options.groot2_port));
      SD_LOG_ACT("viz", "Groot2 已监听端口 %d", options.groot2_port);
    }
    intervention_event_pub_ = create_publisher<sentry_decision_msgs::msg::InterventionEvent>(
        "/decision/intervention", 100);
    intervention_server_ = std::make_shared<sentry_decision_io::InterventionServer>(
        *this, "/decision/manual_override", "/decision/debug",
        [this]() { return state_snapshot_json(); });
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
    // 先应用上一节拍到本拍的干预命令，再取世界快照，使世界覆盖当拍生效。
    apply_intervention_commands(now);
    context_.world = intervention_.apply_world(world_model_.snapshot(now));
    context_.clear_intents();
    context_.apply_strategy(policy_.decide(context_.world));
    for (const auto& intent : intervention_.active_intents(now)) {
      context_.emit(intent);
    }
    const TimePoint tick_begin = SteadyClock::now();
    tree_->tickOnce();
    const double tick_ms =
        std::chrono::duration<double, std::milli>(SteadyClock::now() - tick_begin).count();

    // 每 tick 重建来源：清掉旧干预意图，避免模块关闭后上一 tick 的意图仍生效。
    arbiter_.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter_.clear_source(sentry_decision::SourceId::kSkill);
    arbiter_.clear_source(sentry_decision::SourceId::kIntervention);
    for (const auto& intent : context_.intents) {
      if (!intervention_.allows(intent.field)) {
        continue;  // 运行期模块开关关闭时，丢弃该字段的意图
      }
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
    tree_publisher_->publish(*tree_, tick, tick_ms);
    update_intervention_state(safe_result, now);

    if (max_ticks_ > 0 && tick_count_ >= static_cast<std::uint32_t>(max_ticks_)) {
      rclcpp::shutdown();
    }
  }

  // 干预：tick 边界应用本拍入队的 ROS service / action 命令。
  void apply_intervention_commands(TimePoint now) {
    if (!intervention_server_) {
      return;
    }
    for (const auto& command : intervention_server_->take_commands()) {
      apply_intervention_command(command, now);
    }
  }

  void apply_intervention_command(const sentry_decision_io::InterventionCommand& command,
                                  TimePoint now) {
    using CommandKind = sentry_decision_io::InterventionCommand::Kind;
    // 应用逻辑与回放共用 core::apply_intervention，这里只负责事件与日志。
    sentry_decision::apply_intervention(&intervention_, command, now);
    sentry_decision_msgs::msg::InterventionEvent event;
    event.header.stamp = this->now();
    event.reason = command.reason;
    switch (command.kind) {
      case CommandKind::kIntent:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_INTENT;
        event.field = static_cast<std::uint8_t>(command.intent.field);
        event.value = command.value_text;
        event.lease_sec = static_cast<double>(command.intent.lease.count()) / 1000.0;
        SD_LOG_ACT("intervention", "注入意图 field=%d reason=%s",
                   static_cast<int>(command.intent.field), command.reason.c_str());
        break;
      case CommandKind::kClearIntent:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_CLEAR_INTENT;
        event.field = static_cast<std::uint8_t>(command.intent_field);
        SD_LOG_ACT("intervention", "清除意图 field=%d", static_cast<int>(command.intent_field));
        break;
      case CommandKind::kWorldOverride:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_WORLD_OVERRIDE;
        event.field = static_cast<std::uint8_t>(command.world_field);
        event.value = std::to_string(command.world_value);
        SD_LOG_ACT("intervention", "世界覆盖 field=%s value=%.2f",
                   sentry_decision_io::world_field_name(command.world_field), command.world_value);
        break;
      case CommandKind::kClearWorld:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_CLEAR_WORLD;
        event.field = static_cast<std::uint8_t>(command.world_field);
        break;
      case CommandKind::kModuleSwitch:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_MODULE_SWITCH;
        event.module = command.module;
        event.enabled = command.enabled;
        SD_LOG_ACT("intervention", "模块 %s %s", command.module.c_str(),
                   command.enabled ? "启用" : "禁用");
        break;
      case CommandKind::kClearAll:
        event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_CLEAR_ALL;
        SD_LOG_ACT("intervention", "清空全部干预");
        break;
    }
    intervention_event_pub_->publish(event);
  }

  // 反馈 + 状态快照：向活跃 action goal 报告逐字段胜负，并刷新 list_state 的 JSON。
  void update_intervention_state(const sentry_decision::ArbiterResult& result, TimePoint now) {
    const std::vector<sentry_decision::Intent> active = intervention_.active_intents(now);
    std::vector<sentry_decision::IntentField> active_fields;
    active_fields.reserve(active.size());
    for (const auto& intent : active) {
      active_fields.push_back(intent.field);
    }
    if (intervention_server_) {
      intervention_server_->update_status(result.winners, active_fields);
    }

    std::ostringstream out;
    out << "{\"world\":{\"self_hp\":" << context_.world.referee.self_hp
        << ",\"self_ammo\":" << context_.world.referee.self_ammo
        << ",\"coins\":" << context_.world.referee.coins
        << ",\"game_status\":" << static_cast<int>(context_.world.referee.game_status)
        << ",\"game_time_remaining\":" << context_.world.referee.game_time_remaining
        << ",\"our_outpost_hp\":" << context_.world.referee.our_outpost_hp
        << ",\"enemy_outpost_hp\":" << context_.world.referee.enemy_outpost_hp
        << ",\"enemy_base_hp\":" << context_.world.referee.enemy_base_hp
        << ",\"referee_valid\":" << (context_.world.referee.valid ? "true" : "false") << "}";
    out << ",\"intents\":[";
    bool first = true;
    for (const auto& intent : active) {
      if (!first) {
        out << ",";
      }
      first = false;
      const auto winner = result.winners.find(intent.field);
      const bool effective = winner != result.winners.end() && winner->second == intent.source;
      out << "{\"field\":\"" << sentry_decision_io::intent_field_name(intent.field)
          << "\",\"source\":\"" << sentry_decision_io::source_name(intent.source)
          << "\",\"priority\":" << static_cast<int>(intent.priority)
          << ",\"effective\":" << (effective ? "true" : "false") << "}";
    }
    out << "],\"winners\":{";
    first = true;
    for (const auto& entry : result.winners) {
      if (!first) {
        out << ",";
      }
      first = false;
      out << "\"" << sentry_decision_io::intent_field_name(entry.first) << "\":\""
          << sentry_decision_io::source_name(entry.second) << "\"";
    }
    out << "},\"modules\":{";
    first = true;
    for (const auto& entry : intervention_.module_switches()) {
      if (!first) {
        out << ",";
      }
      first = false;
      out << "\"" << entry.first << "\":" << (entry.second ? "true" : "false");
    }
    out << "},\"world_overrides\":{";
    first = true;
    for (const auto& entry : intervention_.world_overrides()) {
      if (!first) {
        out << ",";
      }
      first = false;
      out << "\"" << sentry_decision_io::world_field_name(entry.first) << "\":" << entry.second;
    }
    out << "},\"safety_emergency\":" << (safety_emergency_ ? "true" : "false") << "}";

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
    }
    sentry_decision::submit_resource_requests(dispatcher_, result.output.resource);
    for (const auto& action : dispatcher_.poll(now)) {
      io_->send_action(action);
    }
  }

  std::shared_ptr<sentry_decision_io::RosIoNode> io_;
  sentry_decision::DecisionContext context_;
  sentry_decision::WorldModel world_model_;
  sentry_decision::IntentArbiter arbiter_;
  sentry_decision::ActionDispatcher dispatcher_;
  sentry_decision::InterventionController intervention_;
  sentry_decision::SafetySupervisor safety_;
  sentry_decision::RuleBasedStrategicPolicy policy_;
  sentry_decision_io::DecisionStatePublisher state_publisher_;
  BT::BehaviorTreeFactory factory_;
  std::unique_ptr<BT::Tree> tree_;
  sentry_decision::NavGoalTracker nav_tracker_;
  bool safety_emergency_ = false;
  std::uint32_t tick_count_ = 0;
  int max_ticks_ = 0;
  std::optional<sentry_decision_viz::TreeStatePublisher> tree_publisher_;
  std::unique_ptr<sentry_decision_viz::Groot2Bridge> groot2_;
  std::shared_ptr<sentry_decision_io::InterventionServer> intervention_server_;
  rclcpp::Publisher<sentry_decision_msgs::msg::InterventionEvent>::SharedPtr
      intervention_event_pub_;
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
