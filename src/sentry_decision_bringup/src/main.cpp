#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_nodes/nodes.hpp"
#include "sentry_decision_sim/nav_simulator.hpp"
#include "sentry_decision_sim/referee_simulator.hpp"

#ifndef DEFAULT_TREE_PATH
#define DEFAULT_TREE_PATH "tree/demo_tree.xml"
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

  sentry_decision::DecisionContext context;

  sentry_decision_sim::RefereeSimulator referee;
  sentry_decision_sim::NavSimulator navigation(2.0, 0.2);
  referee.schedule(Duration{static_cast<std::int64_t>(options.hp_drop_sec * 1000.0)},
                   [](sentry_decision::RefereeState& state) { state.self_hp = 50; });
  sentry_decision::WorldModel world_model(referee, navigation, navigation);

  BT::BehaviorTreeFactory factory;
  if (!options.plugin.empty()) {
    factory.registerFromPlugin(options.plugin);
  } else {
    sentry_decision::register_sentry_nodes(factory);
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
  const Duration period{static_cast<std::int64_t>(1000.0 / options.rate_hz)};
  const TimePoint epoch = SteadyClock::now();
  std::optional<sentry_decision::Point2D> last_goal;

  for (int tick = 0; tick < options.ticks; ++tick) {
    const TimePoint now = epoch + period * tick;
    referee.update(now);
    navigation.update(now);
    context.world = world_model.snapshot(now);
    context.clear_intents();
    tree.tickOnce();

    arbiter.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter.clear_source(sentry_decision::SourceId::kSkill);
    for (const auto& intent : context.intents) {
      arbiter.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter.resolve(now);

    // 用仲裁后的目标驱动伪导航；做边沿检测，避免每 tick 重发导致无法到达。
    if (result.output.nav_goal.has_value()) {
      const sentry_decision::Point2D& goal = *result.output.nav_goal;
      if (!last_goal.has_value() || last_goal->x != goal.x || last_goal->y != goal.y ||
          last_goal->yaw != goal.yaw) {
        navigation.send_goal(goal);
        last_goal = goal;
      }
    } else if (last_goal.has_value()) {
      navigation.cancel_goal();
      last_goal.reset();
    }

    if (result.output.nav_goal.has_value()) {
      SD_LOG_ACT("bringup", "tick %d hp=%d mode=%d goal=(%.2f, %.2f) pos=(%.2f, %.2f)", tick,
                 context.world.referee.self_hp, static_cast<int>(result.output.tactical_mode),
                 result.output.nav_goal->x, result.output.nav_goal->y, navigation.pose().x,
                 navigation.pose().y);
    } else {
      SD_LOG_ACT("bringup", "tick %d hp=%d 无导航目标", tick, context.world.referee.self_hp);
    }

    std::this_thread::sleep_for(period);
  }

  SD_LOG_ACT("bringup", "结束，共 %d tick", options.ticks);
  return 0;
}
