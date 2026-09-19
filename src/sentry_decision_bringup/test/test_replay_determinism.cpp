#include <cstdio>
#include <string>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/replay.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_nodes/nodes.hpp"

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
  DecisionContext context;
  blackboard->set("context", &context);
  BT::Tree tree = factory.createTreeFromFile(tree_path, blackboard);

  IntentArbiter arbiter;
  std::vector<StepResult> steps;
  const Duration period{50};
  for (int tick = 0; tick < 60; ++tick) {
    replay.step(period);
    context.world = model.snapshot(replay.stamp());
    context.clear_intents();
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

  // 表驱动：0~950ms 巡逻去 (1,1)，1000ms 起掉血撤退去 (-2,0)。
  for (std::size_t i = 0; i < 19; ++i) {
    CHECK(first[i].hp == 400);
    CHECK(first[i].mode == TacticalMode::kPatrol);
    CHECK(first[i].has_goal);
    CHECK(first[i].goal_x == 1.0);
    CHECK(first[i].goal_y == 1.0);
  }
  CHECK(first[19].hp == 50);
  CHECK(first[19].mode == TacticalMode::kRetreat);
  CHECK(first[19].has_goal);
  CHECK(first[19].goal_x == -2.0);
  CHECK(first[19].goal_y == 0.0);
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
