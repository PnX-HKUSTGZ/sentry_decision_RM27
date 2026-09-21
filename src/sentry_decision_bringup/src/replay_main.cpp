#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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
#include "sentry_decision_core/replay.hpp"
#include "sentry_decision_core/safety_supervisor.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_io/rosbag_replay.hpp"
#include "sentry_decision_nodes/nodes.hpp"
#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"

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
  std::string bag;
  int ticks = 0;  // 0 表示回放到数据结束
  double rate_hz = 20.0;
  std::string tree = DEFAULT_TREE_PATH;
  std::string config = DEFAULT_CONFIG_PATH;
  std::string odom_topic = "/aft_mapped_to_init";
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
    if (arg == "--bag") {
      options.bag = next("--bag");
    } else if (arg == "--ticks") {
      options.ticks = std::stoi(next("--ticks"));
    } else if (arg == "--rate") {
      options.rate_hz = std::stod(next("--rate"));
    } else if (arg == "--tree") {
      options.tree = next("--tree");
    } else if (arg == "--config") {
      options.config = next("--config");
    } else if (arg == "--odom-topic") {
      options.odom_topic = next("--odom-topic");
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      std::exit(2);
    }
  }
  if (options.bag.empty()) {
    std::cerr << "必须提供 --bag <rosbag2 路径>\n";
    std::exit(2);
  }
  if (!(options.rate_hz > 0.0) || options.rate_hz > 1000.0) {
    std::cerr << "参数 --rate 必须在 (0, 1000] Hz 之间\n";
    std::exit(2);
  }
  return options;
}

}  // namespace

// 离线回放入口：读 rosbag2 -> core 确定性重放 -> 行为树 / 仲裁 / 安全，
// 并在原时刻注入录到的人工干预。不启动 ROS 节点，只做数据处理。
int main(int argc, char** argv) {
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
    return 1;
  }
  SD_LOG_ACT("config", "%s", sentry_decision_bringup::format_config(loaded.config).c_str());

  sentry_decision_io::ReplayTopics topics;
  topics.odometry = options.odom_topic;
  sentry_decision::ReplayData data;
  try {
    data = sentry_decision_io::load_replay_data(options.bag, topics);
  } catch (const std::exception& ex) {
    std::cerr << "读取 rosbag 失败: " << ex.what() << "\n";
    return 1;
  }
  if (data.referee.empty()) {
    std::cerr << "bag 中没有裁判数据（检查上行话题是否录制）\n";
    return 1;
  }
  SD_LOG_ACT("replay", "载入 bag: referee=%zu odometry=%zu navigation=%zu interventions=%zu",
             data.referee.size(), data.odometry.size(), data.navigation.size(),
             data.interventions.size());

  sentry_decision::DecisionContext context;
  context.config = &loaded.config;
  const sentry_decision::RuleBasedStrategicPolicy policy =
      sentry_decision::RuleBasedStrategicPolicy::from_config(loaded.config);

  BT::BehaviorTreeFactory factory;
  std::vector<std::string> errors;
  sentry_decision_bringup::TreeSetupOptions tree_options;
  tree_options.module_lib_dir = DEFAULT_MODULE_LIB_DIR;
  if (!sentry_decision_bringup::setup_tree_factory(factory, options.tree, &loaded.config, &errors,
                                                   tree_options)) {
    std::cerr << "行为树校验失败: " << options.tree << "\n";
    for (const auto& error : errors) {
      std::cerr << "  - " << error << "\n";
    }
    return 1;
  }
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", &context);
  BT::Tree tree = factory.createTreeFromFile(options.tree, blackboard);

  sentry_decision::ReplaySource replay(std::move(data));
  sentry_decision::WorldModel model(replay, replay, replay);
  sentry_decision::IntentArbiter arbiter;
  sentry_decision::ActionDispatcher dispatcher;
  sentry_decision::InterventionController intervention;
  sentry_decision::SafetySupervisor safety;
  sentry_decision::NavGoalTracker nav_tracker;

  const Duration period{static_cast<std::int64_t>(1000.0 / options.rate_hz)};
  const int max_ticks = options.ticks > 0 ? options.ticks : 1000000;
  sentry_decision::TacticalMode last_mode = sentry_decision::TacticalMode::kUnknown;
  std::uint32_t mode_changes = 0;
  std::uint32_t applied_interventions = 0;

  for (int tick = 0; tick < max_ticks; ++tick) {
    replay.step(period);
    const TimePoint now = replay.stamp();

    for (const auto& command : replay.interventions()) {
      sentry_decision::apply_intervention(&intervention, command, now);
      ++applied_interventions;
      SD_LOG_ACT("replay", "t=%.1fs 注入干预 kind=%d", replay.now().count() / 1000.0,
                 static_cast<int>(command.kind));
    }

    context.world = intervention.apply_world(model.snapshot(now));
    context.clear_intents();
    context.apply_strategy(policy.decide(context.world));
    for (const auto& intent : intervention.active_intents(now)) {
      context.emit(intent);
    }
    tree.tickOnce();

    arbiter.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter.clear_source(sentry_decision::SourceId::kSkill);
    arbiter.clear_source(sentry_decision::SourceId::kIntervention);
    for (const auto& intent : context.intents) {
      if (!intervention.allows(intent.field)) {
        continue;
      }
      arbiter.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter.resolve(now);
    const sentry_decision::SafetyResult safe = safety.apply(context.world, result.output);
    sentry_decision::ArbiterResult safe_result = result;
    safe_result.output = safe.output;

    const sentry_decision::NavGoalTracker::Step nav_step =
        nav_tracker.update(safe_result.output.nav_goal);
    if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kSend &&
        nav_step.goal.has_value()) {
      replay.send_goal(*nav_step.goal);
    } else if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kCancel) {
      replay.cancel_goal();
    }
    sentry_decision::submit_resource_requests(dispatcher, safe_result.output.resource);
    (void)dispatcher.poll(now);  // 回放不真实下发动作，只推进状态机

    if (safe_result.output.tactical_mode != last_mode) {
      SD_LOG_ACT("replay", "t=%.1fs mode %d -> %d", replay.now().count() / 1000.0,
                 static_cast<int>(last_mode), static_cast<int>(safe_result.output.tactical_mode));
      last_mode = safe_result.output.tactical_mode;
      ++mode_changes;
    }

    if (replay.finished()) {
      break;
    }
  }

  SD_LOG_ACT("replay",
             "回放结束: tick=%u mode_changes=%u interventions=%u sent_goals=%u canceled=%u",
             replay.tick(), mode_changes, applied_interventions, replay.sent_goals(),
             replay.canceled_goals());
  return 0;
}
