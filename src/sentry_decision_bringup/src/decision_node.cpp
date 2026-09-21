#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_bringup/config_loader.hpp"
#include "sentry_decision_bringup/tree_loader.hpp"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_io/decision_state_publisher.hpp"
#include "sentry_decision_io/ros_io_node.hpp"
#include "sentry_decision_nodes/nodes.hpp"

#ifndef DEFAULT_TREE_PATH
#define DEFAULT_TREE_PATH "tree/root.xml"
#endif
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "config/profiles.yaml"
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
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      std::exit(2);
    }
  }
  if (!(options.rate_hz > 0.0) || options.rate_hz > 1000.0) {
    std::cerr << "参数 --rate 必须在 (0, 1000] Hz 之间\n";
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
    build_tree(options.tree, options.plugin);
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
    if (!sentry_decision_bringup::setup_tree_factory(factory_, tree_path, context_.config, &errors,
                                                     plugin.empty())) {
      throw std::runtime_error("行为树校验失败: " + join_errors(errors));
    }
    auto blackboard = BT::Blackboard::create();
    blackboard->set("context", &context_);
    tree_ = std::make_unique<BT::Tree>(factory_.createTreeFromFile(tree_path, blackboard));
  }

  void tick() {
    const TimePoint now = SteadyClock::now();
    context_.world = world_model_.snapshot(now);
    context_.clear_intents();
    tree_->tickOnce();

    arbiter_.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter_.clear_source(sentry_decision::SourceId::kSkill);
    for (const auto& intent : context_.intents) {
      arbiter_.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter_.resolve(now);

    apply_nav(result);
    if (result.output.cmd_vel.has_value()) {
      io_->set_velocity(*result.output.cmd_vel);
    }
    apply_resource(result.output.resource);

    state_publisher_.publish(context_.world, result, tick_count_++);

    if (max_ticks_ > 0 && tick_count_ >= static_cast<std::uint32_t>(max_ticks_)) {
      rclcpp::shutdown();
    }
  }

  // 导航目标边沿检测：只有目标变化时才下发，避免每 tick 重发导致无法到达。
  void apply_nav(const sentry_decision::ArbiterResult& result) {
    if (result.output.nav_goal.has_value()) {
      const sentry_decision::Point2D& goal = *result.output.nav_goal;
      if (!last_goal_.has_value() || last_goal_->x != goal.x || last_goal_->y != goal.y ||
          last_goal_->yaw != goal.yaw) {
        io_->send_goal(goal);
        last_goal_ = goal;
      }
    } else if (last_goal_.has_value()) {
      io_->cancel_goal();
      last_goal_.reset();
    }
  }

  // 资源请求 -> 一次性决策动作：只在请求变化时发送，避免逐 tick 重复兑换。
  void apply_resource(const sentry_decision::ResourceRequest& resource) {
    const auto send = [this](sentry_decision::DecisionActionKind kind, int value) {
      sentry_decision::DecisionAction action;
      action.kind = kind;
      action.mode = sentry_decision::ActionMode::kOneShot;
      action.value = value;
      action.request_id = ++request_id_;
      io_->send_action(action);
    };
    if (resource.ammo > 0 && resource.ammo != last_resource_.ammo) {
      send(sentry_decision::DecisionActionKind::kAmmoExchange, resource.ammo);
    }
    if (resource.hp > 0 && resource.hp != last_resource_.hp) {
      send(sentry_decision::DecisionActionKind::kHpExchange, resource.hp);
    }
    if (resource.revive && !last_resource_.revive) {
      send(sentry_decision::DecisionActionKind::kFreeResurrect, 0);
    }
    last_resource_ = resource;
  }

  std::shared_ptr<sentry_decision_io::RosIoNode> io_;
  sentry_decision::DecisionContext context_;
  sentry_decision::WorldModel world_model_;
  sentry_decision::IntentArbiter arbiter_;
  sentry_decision_io::DecisionStatePublisher state_publisher_;
  BT::BehaviorTreeFactory factory_;
  std::unique_ptr<BT::Tree> tree_;
  std::optional<sentry_decision::Point2D> last_goal_;
  sentry_decision::ResourceRequest last_resource_{};
  std::uint32_t request_id_ = 0;
  std::uint32_t tick_count_ = 0;
  int max_ticks_ = 0;
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

  auto io = std::make_shared<sentry_decision_io::RosIoNode>();
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
