#include <cstdio>
#include <optional>
#include <string>
#include <variant>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"
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

const char* kTree = R"xml(
<root BTCPP_format="4">
  <BehaviorTree ID="MainTree">
    <ReactiveFallback name="root">
      <Sequence name="retreat">
        <IfLowHp hp_key="nav.retreat_hp"/>
        <EmitNavGoalFromPoint point="home"/>
      </Sequence>
      <Sequence name="patrol">
        <EmitNavGoalFromPoint point="patrol_a"/>
      </Sequence>
    </ReactiveFallback>
  </BehaviorTree>
</root>
)xml";

const char* kPatrolTree = R"xml(
<root BTCPP_format="4">
  <BehaviorTree ID="PatrolTree">
    <ReactiveSequence name="patrol">
      <PatrolLoop points="p1,p2,p3" dwell_key="nav.patrol_dwell_s" loop_id="test"/>
    </ReactiveSequence>
  </BehaviorTree>
</root>
)xml";

PolicyConfig make_config() {
  PolicyConfig config;
  config.points["home"] = Point2D{-2.0, 0.0, 0.0};
  config.points["patrol_a"] = Point2D{1.0, 1.0, 0.0};
  config.numbers["nav.retreat_hp"] = 120.0;
  return config;
}

PolicyConfig make_patrol_config() {
  PolicyConfig config;
  config.points["p1"] = Point2D{1.0, 0.0, 0.0};
  config.points["p2"] = Point2D{2.0, 0.0, 0.0};
  config.points["p3"] = Point2D{3.0, 0.0, 0.0};
  config.numbers["nav.patrol_dwell_s"] = 2.0;
  return config;
}

BT::Tree make_tree(DecisionContext* context) {
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", context);
  return factory.createTreeFromText(kTree, blackboard);
}

BT::Tree make_patrol_tree(DecisionContext* context) {
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", context);
  return factory.createTreeFromText(kPatrolTree, blackboard);
}

void test_branch_switch() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world.referee.valid = true;
  context.world.referee.self_hp = 50;
  BT::Tree tree = make_tree(&context);

  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(context.intents[0].field == IntentField::kNavGoal);
  CHECK(std::get<Point2D>(context.intents[0].value).x == -2.0);

  context.world.referee.self_hp = 300;
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(std::get<Point2D>(context.intents[0].value).x == 1.0);
}

void test_invalid_referee_falls_back() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world.referee.valid = false;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(std::get<Point2D>(context.intents[0].value).x == 1.0);
}

// 配置缺 key 时条件失败，回退到巡逻分支而不是崩溃。
void test_missing_config_key_falls_back() {
  PolicyConfig config = make_config();
  config.numbers.clear();
  DecisionContext context;
  context.config = &config;
  context.world.referee.valid = true;
  context.world.referee.self_hp = 50;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(std::get<Point2D>(context.intents[0].value).x == 1.0);
}

// 巡逻：对当前点发目标，到点停留 dwell 秒后切下一点，到末尾循环回第一个点。
void test_patrol_loop() {
  PolicyConfig config = make_patrol_config();
  DecisionContext context;
  context.config = &config;
  context.world.referee.valid = true;
  BT::Tree tree = make_patrol_tree(&context);
  const TimePoint t0{};

  const auto tick_goal = [&](TimePoint now, bool reached) -> std::optional<Point2D> {
    context.world.stamp = now;
    context.world.nav.reached = reached;
    context.clear_intents();
    tree.tickOnce();
    if (context.intents.empty()) {
      return std::nullopt;
    }
    return std::get<Point2D>(context.intents[0].value);
  };

  // 未到点：持续去 p1。
  CHECK(tick_goal(t0, false)->x == 1.0);
  CHECK(tick_goal(t0 + Duration{1000}, false)->x == 1.0);

  // 到达后停留：dwell 未满仍去 p1。
  CHECK(tick_goal(t0 + Duration{2000}, true)->x == 1.0);
  CHECK(tick_goal(t0 + Duration{3000}, true)->x == 1.0);
  // dwell 满 2s -> 切 p2。
  CHECK(tick_goal(t0 + Duration{5000}, true)->x == 2.0);

  // 到 p2 后停留满 -> p3。
  CHECK(tick_goal(t0 + Duration{6000}, true)->x == 2.0);
  CHECK(tick_goal(t0 + Duration{9000}, true)->x == 3.0);
  // p3 停留满 -> 循环回 p1。
  CHECK(tick_goal(t0 + Duration{10000}, true)->x == 3.0);
  CHECK(tick_goal(t0 + Duration{13000}, true)->x == 1.0);
}

}  // namespace

int main() {
  test_branch_switch();
  test_invalid_referee_falls_back();
  test_missing_config_key_falls_back();
  test_patrol_loop();
  if (g_failures == 0) {
    std::printf("all nodes tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
