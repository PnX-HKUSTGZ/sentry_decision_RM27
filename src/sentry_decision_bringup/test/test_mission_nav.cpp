#include <cstdio>
#include <optional>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_nodes/nodes.hpp"
#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"

using namespace sentry_decision;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

PolicyConfig make_config() {
  PolicyConfig config;
  config.points["home"] = Point2D{-6.0, 4.0, 0.0};
  config.points["fort"] = Point2D{-5.0, 3.0, 0.0};
  config.points["central_highland"] = Point2D{-1.1, -1.1, 0.0};
  config.points["enemy_outpost"] = Point2D{1.1, 1.1, 0.0};
  config.points["patrol_a"] = Point2D{0.0, 1.1, 0.0};
  config.numbers["nav.retreat_hp"] = 50.0;
  config.numbers["nav.low_ammo"] = 50.0;
  config.numbers["strategic.attack_window_min_remaining"] = 0.0;
  config.numbers["strategic.attack_window_max_remaining"] = 420.0;
  return config;
}

WorldState make_world() {
  WorldState world;
  world.referee.valid = true;
  world.referee.self_hp = 400;
  world.referee.self_ammo = 100;
  world.referee.our_outpost_hp = 1500;
  world.referee.enemy_outpost_hp = 1500;
  world.referee.game_time_remaining = 420;
  world.stamp = TimePoint{} + Duration{1000};
  return world;
}

// 驱动一棵完整的 root.xml：战略求值 -> 树 tick -> 仲裁，返回导航目标。
std::optional<Point2D> run_goal(const std::string& tree_path, const PolicyConfig& config,
                                const WorldState& world) {
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  DecisionContext context;
  context.config = &config;
  context.world = world;
  blackboard->set("context", &context);
  BT::Tree tree = factory.createTreeFromFile(tree_path, blackboard);

  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);
  context.clear_intents();
  context.apply_strategy(policy.decide(context.world));
  tree.tickOnce();

  IntentArbiter arbiter;
  for (const auto& intent : context.intents) {
    arbiter.submit(intent);
  }
  const ArbiterResult result = arbiter.resolve(context.world.stamp);
  return result.output.nav_goal;
}

void expect_goal(const std::string& tree_path, const PolicyConfig& config, const WorldState& world,
                 double x, double y, const char* label) {
  const std::optional<Point2D> goal = run_goal(tree_path, config, world);
  CHECK(goal.has_value());
  if (!goal.has_value()) {
    std::printf("  场景 [%s]: 无导航目标\n", label);
    return;
  }
  CHECK(goal->x == x);
  CHECK(goal->y == y);
  if (goal->x != x || goal->y != y) {
    std::printf("  场景 [%s]: 期望 (%.2f, %.2f) 实际 (%.2f, %.2f)\n", label, x, y, goal->x,
                goal->y);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("用法: %s <root.xml>\n", argv[0]);
    return 2;
  }
  const std::string tree_path = argv[1];
  const PolicyConfig config = make_config();

  WorldState world = make_world();
  expect_goal(tree_path, config, world, 1.1, 1.1, "双方前哨存活 -> 进攻敌方前哨");

  world = make_world();
  world.referee.self_hp = 50;
  expect_goal(tree_path, config, world, -6.0, 4.0, "低血 -> 撤退回家");

  world = make_world();
  world.referee.self_ammo = 10;
  expect_goal(tree_path, config, world, -6.0, 4.0, "低弹 -> 回补给");

  world = make_world();
  world.referee.our_outpost_hp = 0;
  expect_goal(tree_path, config, world, -5.0, 3.0, "我方前哨阵亡 -> 守堡垒");

  world = make_world();
  world.referee.enemy_outpost_hp = 0;
  expect_goal(tree_path, config, world, -1.1, -1.1, "敌方前哨被毁 -> 中央高地");

  world = make_world();
  world.referee.game_time_remaining = 500;  // 超出进攻窗口
  expect_goal(tree_path, config, world, 0.0, 1.1, "超出进攻窗口 -> 巡逻");

  if (g_failures == 0) {
    std::printf("all mission nav tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
