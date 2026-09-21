#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/nav_goal_tracker.hpp"
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
  return world;
}

struct TickResult {
  std::optional<Point2D> goal;
  NavGoalTracker::Step step;
  std::size_t nav_intents = 0;
};

TickResult tick(BT::Tree& tree, DecisionContext& context, const PolicyConfig& config,
                const RuleBasedStrategicPolicy& policy, NavGoalTracker& tracker,
                const WorldState& world, TimePoint now) {
  context.world = world;
  context.world.stamp = now;
  context.clear_intents();
  context.apply_strategy(policy.decide(context.world));
  tree.tickOnce();

  IntentArbiter arbiter;
  std::size_t nav_intents = 0;
  for (const auto& intent : context.intents) {
    arbiter.submit(intent);
    if (intent.field == IntentField::kNavGoal) {
      ++nav_intents;
    }
  }
  const ArbiterResult result = arbiter.resolve(now);

  TickResult out;
  out.goal = result.output.nav_goal;
  out.nav_intents = nav_intents;
  out.step = tracker.update(result.output.nav_goal);
  return out;
}

void test_preemption_contract(const std::string& tree_path) {
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  const PolicyConfig config = make_config();
  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);
  DecisionContext context;
  context.config = &config;
  blackboard->set("context", &context);
  BT::Tree tree = factory.createTreeFromFile(tree_path, blackboard);
  NavGoalTracker tracker;

  const TimePoint t0{};

  // 进攻 -> 下发敌方前哨。
  const WorldState attack = make_world();
  TickResult r = tick(tree, context, config, policy, tracker, attack, t0);
  CHECK(r.step.decision == NavGoalTracker::Decision::kSend);
  CHECK(r.step.goal.has_value());
  CHECK(r.step.goal->x == 1.1);
  CHECK(r.nav_intents <= 1);  // 仲裁后每 tick 至多一个 nav_goal

  // 同状态：不重发。
  r = tick(tree, context, config, policy, tracker, attack, t0 + Duration{50});
  CHECK(r.step.decision == NavGoalTracker::Decision::kNone);

  // 低血：抢占进攻，改派回家。
  WorldState retreat = make_world();
  retreat.referee.self_hp = 50;
  r = tick(tree, context, config, policy, tracker, retreat, t0 + Duration{100});
  CHECK(r.step.decision == NavGoalTracker::Decision::kSend);
  CHECK(r.step.goal.has_value());
  CHECK(r.step.goal->x == -6.0);

  // 持续低血：不重发。
  r = tick(tree, context, config, policy, tracker, retreat, t0 + Duration{150});
  CHECK(r.step.decision == NavGoalTracker::Decision::kNone);

  // 血量恢复：进攻任务重新接管，改派敌方前哨。
  r = tick(tree, context, config, policy, tracker, attack, t0 + Duration{200});
  CHECK(r.step.decision == NavGoalTracker::Decision::kSend);
  CHECK(r.step.goal.has_value());
  CHECK(r.step.goal->x == 1.1);

  // 目标被撤销（例如安全急停）：取消。
  r = tick(tree, context, config, policy, tracker, attack, t0 + Duration{250});
  CHECK(r.step.decision == NavGoalTracker::Decision::kNone);
  const NavGoalTracker::Step cancel = tracker.update(std::nullopt);
  CHECK(cancel.decision == NavGoalTracker::Decision::kCancel);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("用法: %s <root.xml>\n", argv[0]);
    return 2;
  }
  test_preemption_contract(argv[1]);
  if (g_failures == 0) {
    std::printf("all preemption tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
