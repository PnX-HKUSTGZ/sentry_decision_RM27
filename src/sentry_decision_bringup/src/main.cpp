#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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
#include "sentry_decision_nodes/nodes.hpp"
#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"
#include "sentry_decision_sim/decision_actuator_sim.hpp"
#include "sentry_decision_sim/nav_simulator.hpp"
#include "sentry_decision_sim/referee_simulator.hpp"

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
  int ticks = 50;
  double rate_hz = 20.0;
  double hp_drop_sec = 1.0;
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
    if (arg == "--ticks") {
      options.ticks = std::stoi(next("--ticks"));
    } else if (arg == "--rate") {
      options.rate_hz = std::stod(next("--rate"));
    } else if (arg == "--hp-drop") {
      options.hp_drop_sec = std::stod(next("--hp-drop"));
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
  // 频率决定仿真周期：过小导致周期为 0（时间不前进），非正数会产生非法换算。
  if (!(options.rate_hz > 0.0) || options.rate_hz > 1000.0) {
    std::cerr << "参数 --rate 必须在 (0, 1000] Hz 之间\n";
    std::exit(2);
  }
  return options;
}

}  // namespace

// 本地仿真入口：用 dummy 裁判系统与伪导航驱动信念层，不依赖下位机通信包与导航仓库。
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

  sentry_decision::DecisionContext context;
  context.config = &loaded.config;
  const sentry_decision::RuleBasedStrategicPolicy policy =
      sentry_decision::RuleBasedStrategicPolicy::from_config(loaded.config);

  sentry_decision_sim::RefereeSimulator referee;
  sentry_decision_sim::NavSimulator navigation(2.0, 0.2);
  referee.schedule(Duration{static_cast<std::int64_t>(options.hp_drop_sec * 1000.0)},
                   [](sentry_decision::RefereeState& state) { state.self_hp = 50; });
  sentry_decision::WorldModel world_model(referee, navigation, navigation);

  BT::BehaviorTreeFactory factory;
  std::vector<std::string> errors;
  if (!options.plugin.empty()) {
    factory.registerFromPlugin(options.plugin);
  }
  if (!sentry_decision_bringup::setup_tree_factory(factory, options.tree, &loaded.config, &errors,
                                                   DEFAULT_MODULE_LIB_DIR,
                                                   options.plugin.empty())) {
    std::cerr << "行为树校验失败: " << options.tree << "\n";
    for (const auto& error : errors) {
      std::cerr << "  - " << error << "\n";
    }
    return 1;
  }

  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", &context);

  BT::Tree tree = [&]() {
    try {
      return factory.createTreeFromFile(options.tree, blackboard);
    } catch (const std::exception& ex) {
      std::cerr << "加载行为树失败: " << options.tree << "\n" << ex.what() << "\n";
      std::exit(1);
    }
  }();

  sentry_decision::IntentArbiter arbiter;
  sentry_decision::ActionDispatcher dispatcher;
  sentry_decision::InterventionController intervention;
  sentry_decision::SafetySupervisor safety;
  sentry_decision_sim::DecisionActuatorSim actuator(2);
  const Duration period{static_cast<std::int64_t>(1000.0 / options.rate_hz)};
  const TimePoint epoch = SteadyClock::now();
  sentry_decision::NavGoalTracker nav_tracker;
  bool safety_emergency = false;

  for (int tick = 0; tick < options.ticks; ++tick) {
    const TimePoint now = epoch + period * tick;
    referee.update(now);
    navigation.update(now);
    context.world = intervention.apply_world(world_model.snapshot(now));
    context.clear_intents();
    context.apply_strategy(policy.decide(context.world));
    for (const auto& intent : intervention.active_intents(now)) {
      context.emit(intent);
    }
    tree.tickOnce();

    arbiter.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter.clear_source(sentry_decision::SourceId::kSkill);
    for (const auto& intent : context.intents) {
      arbiter.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter.resolve(now);

    // 安全监督：仲裁后做最终限幅与急停兜底。
    const sentry_decision::SafetyResult safe = safety.apply(context.world, result.output);
    if (safe.emergency != safety_emergency) {
      if (safe.emergency) {
        SD_LOG_WARN("safety", "触发急停");
      } else {
        SD_LOG_ACT("safety", "急停解除");
      }
      safety_emergency = safe.emergency;
    }
    sentry_decision::ArbiterResult safe_result = result;
    safe_result.output = safe.output;

    // 用仲裁后的目标驱动伪导航；NavGoalTracker 负责边沿 / 取消契约。
    const sentry_decision::NavGoalTracker::Step nav_step =
        nav_tracker.update(safe_result.output.nav_goal);
    if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kSend &&
        nav_step.goal.has_value()) {
      navigation.send_goal(*nav_step.goal);
    } else if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kCancel) {
      navigation.cancel_goal();
    }

    // 决策动作：本地模拟执行端回执，离线跑通 one-shot / ack 闭环。
    for (const auto& ack : actuator.take_acks()) {
      dispatcher.on_ack(ack);
    }
    sentry_decision::submit_resource_requests(dispatcher, safe_result.output.resource);
    for (const auto& action : dispatcher.poll(now)) {
      actuator.send_action(action);
    }
    actuator.update();

    if (safe_result.output.nav_goal.has_value()) {
      SD_LOG_ACT("bringup", "tick %d hp=%d mode=%d goal=(%.2f, %.2f) pos=(%.2f, %.2f)", tick,
                 context.world.referee.self_hp, static_cast<int>(safe_result.output.tactical_mode),
                 safe_result.output.nav_goal->x, safe_result.output.nav_goal->y,
                 navigation.pose().x, navigation.pose().y);
    } else {
      SD_LOG_ACT("bringup", "tick %d hp=%d 无导航目标", tick, context.world.referee.self_hp);
    }

    std::this_thread::sleep_for(period);
  }

  SD_LOG_ACT("bringup", "结束，共 %d tick", options.ticks);
  return 0;
}
