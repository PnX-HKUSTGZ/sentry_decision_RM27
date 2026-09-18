#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_nodes/nodes.hpp"

#ifndef DEFAULT_TREE_PATH
#define DEFAULT_TREE_PATH "tree/demo_tree.xml"
#endif

namespace {

using sentry_decision::SteadyClock;

struct Options {
  int ticks = 50;
  double rate_hz = 20.0;
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
    } else if (arg == "--tree") {
      options.tree = next("--tree");
    } else if (arg == "--plugin") {
      options.plugin = next("--plugin");
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      std::exit(2);
    }
  }
  return options;
}

// 演示用裁判源：启动 1 秒后掉血，用于触发撤退分支切换。
struct DemoReferee : sentry_decision::RefereeSource {
  SteadyClock::time_point start = SteadyClock::now();

  bool referee(sentry_decision::RefereeState* out) const override {
    const auto now = SteadyClock::now();
    *out = sentry_decision::RefereeState{};
    out->stamp = now;
    out->valid = true;
    out->self_hp = std::chrono::duration<double>(now - start).count() < 1.0 ? 300 : 50;
    out->self_ammo = 100;
    return true;
  }
};

struct DemoOdometry : sentry_decision::OdometrySource {
  bool odometry(sentry_decision::SelfState* out) const override {
    *out = sentry_decision::SelfState{};
    out->stamp = SteadyClock::now();
    out->valid = true;
    return true;
  }
};

struct DemoNavigation : sentry_decision::NavigationSink {
  sentry_decision::NavState state;

  void send_goal(const sentry_decision::Point2D& goal) override {
    state.current_goal = goal;
    state.stamp = SteadyClock::now();
    state.valid = true;
  }

  void cancel_goal() override {
    state.current_goal.reset();
    state.stamp = SteadyClock::now();
    state.valid = true;
  }

  sentry_decision::NavState status() const override {
    return state;
  }
};

}  // namespace

int main(int argc, char** argv) {
  const Options options = parse_options(argc, argv);

  auto console = std::make_shared<sentry_decision::ConsoleSink>(false);
  auto& logger = sentry_decision::Logger::instance();
  logger.clear_sinks();
  logger.add_short_sink(console);
  logger.set_short_min_level(sentry_decision::LogLevel::kAct);

  sentry_decision::DecisionContext context;

  DemoReferee referee;
  DemoOdometry odometry;
  DemoNavigation navigation;
  sentry_decision::WorldModel world_model(referee, odometry, navigation);

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
  const std::chrono::duration<double> period(1.0 / options.rate_hz);

  for (int tick = 0; tick < options.ticks; ++tick) {
    const auto now = SteadyClock::now();
    context.world = world_model.snapshot(now);
    context.clear_intents();
    tree.tickOnce();

    arbiter.clear_source(sentry_decision::SourceId::kStrategic);
    arbiter.clear_source(sentry_decision::SourceId::kSkill);
    for (const auto& intent : context.intents) {
      arbiter.submit(intent);
    }
    const sentry_decision::ArbiterResult result = arbiter.resolve(now);

    if (result.output.nav_goal.has_value()) {
      SD_LOG_ACT("bringup", "tick %d hp=%d mode=%d goal=(%.2f, %.2f)", tick,
                 context.world.referee.self_hp, static_cast<int>(result.output.tactical_mode),
                 result.output.nav_goal->x, result.output.nav_goal->y);
    } else {
      SD_LOG_ACT("bringup", "tick %d hp=%d 无导航目标", tick, context.world.referee.self_hp);
    }

    std::this_thread::sleep_for(period);
  }

  SD_LOG_ACT("bringup", "结束，共 %d tick", options.ticks);
  return 0;
}
