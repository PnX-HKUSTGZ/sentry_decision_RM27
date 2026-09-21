#include <cstdio>
#include <string>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/replay.hpp"
#include "sentry_decision_core/world_model.hpp"
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

// 与 tree/ 骨架匹配的最小配置：命名点 + 撤退阈值。
PolicyConfig make_config() {
  PolicyConfig config;
  config.points["home"] = Point2D{-5.0, 3.0, 0.0};
  config.points["fort"] = Point2D{-5.0, 3.0, 0.0};
  config.points["central_highland"] = Point2D{-1.1, -1.1, 0.0};
  config.points["enemy_outpost"] = Point2D{1.1, 1.1, 0.0};
  config.points["patrol_a"] = Point2D{1.1, 1.1, 0.0};
  config.numbers["nav.retreat_hp"] = 50.0;
  config.numbers["nav.low_ammo"] = 50.0;
  return config;
}

// 每 tick 录下的决策输出，用于比较两次回放是否完全一致。
struct StepResult {
  int hp = 0;
  TacticalMode mode = TacticalMode::kUnknown;
  bool has_goal = false;
  double goal_x = 0.0;
  double goal_y = 0.0;

  bool operator==(const StepResult& other) const {
    return hp == other.hp && mode == other.mode && has_goal == other.has_goal &&
           goal_x == other.goal_x && goal_y == other.goal_y;
  }
};

ReplayData make_data() {
  ReplayData data;

  RefereeState referee;
  referee.valid = true;
  referee.self_hp = 400;
  referee.self_ammo = 100;
  referee.our_outpost_hp = 1500;
  referee.enemy_outpost_hp = 1500;
  referee.game_time_remaining = 420;
  data.referee.push_back({Duration{0}, referee});

  RefereeState hurt = referee;
  hurt.self_hp = 50;
  data.referee.push_back({Duration{1000}, hurt});

  SelfState self;
  self.valid = true;
  data.odometry.push_back({Duration{0}, self});

  NavState nav;
  nav.valid = true;
  data.navigation.push_back({Duration{0}, nav});
  return data;
}

std::vector<StepResult> run(const ReplayData& data, const std::string& tree_path, TimePoint epoch) {
  ReplaySource replay(data, epoch);
  WorldModel model(replay, replay, replay);

  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  PolicyConfig config = make_config();
  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);
  DecisionContext context;
  context.config = &config;
  blackboard->set("context", &context);
  BT::Tree tree = factory.createTreeFromFile(tree_path, blackboard);

  IntentArbiter arbiter;
  std::vector<StepResult> steps;
  const Duration period{50};
  for (int tick = 0; tick < 60; ++tick) {
    replay.step(period);
    context.world = model.snapshot(replay.stamp());
    context.clear_intents();
    context.apply_strategy(policy.decide(context.world));
    tree.tickOnce();

    arbiter.clear_source(SourceId::kStrategic);
    arbiter.clear_source(SourceId::kSkill);
    for (const auto& intent : context.intents) {
      arbiter.submit(intent);
    }
    const ArbiterResult result = arbiter.resolve(replay.stamp());

    StepResult step;
    step.hp = context.world.referee.self_hp;
    step.mode = result.output.tactical_mode;
    step.has_goal = result.output.nav_goal.has_value();
    if (result.output.nav_goal.has_value()) {
      step.goal_x = result.output.nav_goal->x;
      step.goal_y = result.output.nav_goal->y;
    }
    steps.push_back(step);
  }
  return steps;
}

void test_replay_is_deterministic(const std::string& tree_path) {
  const ReplayData data = make_data();
  const TimePoint epoch = TimePoint{} + Duration{1000};

  const std::vector<StepResult> first = run(data, tree_path, epoch);
  const std::vector<StepResult> second = run(data, tree_path, epoch);

  CHECK(first.size() == 60);
  CHECK(first == second);

  // 表驱动：0~950ms 满血、去 patrol_a；1000ms 起低血撤退、去 home。
  // 前若干 tick 裁判数据尚未在有效期内（stale），战略层输出 kUnknown；
  // 数据有效后满血双方前哨在场 -> 进攻。
  for (std::size_t i = 0; i < 19; ++i) {
    CHECK(first[i].hp == 400);
    CHECK(first[i].has_goal);
    CHECK(first[i].goal_x == 1.1);
    CHECK(first[i].goal_y == 1.1);
  }
  CHECK(first[0].mode == TacticalMode::kAttack);
  CHECK(first[19].hp == 50);
  CHECK(first[19].mode == TacticalMode::kRetreat);
  CHECK(first[19].has_goal);
  CHECK(first[19].goal_x == -5.0);
  CHECK(first[19].goal_y == 3.0);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("用法: %s <tree.xml>\n", argv[0]);
    return 2;
  }
  test_replay_is_deterministic(argv[1]);
  if (g_failures == 0) {
    std::printf("all bringup replay determinism tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
