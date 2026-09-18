#include <cstdio>
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
        <CheckLowHp hp_threshold="120"/>
        <EmitTacticalMode mode="4"/>
        <EmitNavGoal x="-2.0" y="0.0"/>
      </Sequence>
      <Sequence name="patrol">
        <EmitTacticalMode mode="1"/>
        <EmitNavGoal x="1.0" y="1.0"/>
      </Sequence>
    </ReactiveFallback>
  </BehaviorTree>
</root>
)xml";

void test_branch_switch() {
  DecisionContext context;
  context.world.referee.valid = true;
  context.world.referee.self_hp = 50;

  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", &context);
  auto tree = factory.createTreeFromText(kTree, blackboard);

  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 2);
  CHECK(context.intents[0].field == IntentField::kTacticalMode);
  CHECK(std::get<TacticalMode>(context.intents[0].value) == TacticalMode::kRetreat);
  CHECK(context.intents[1].field == IntentField::kNavGoal);
  CHECK(std::get<Point2D>(context.intents[1].value).x == -2.0);

  context.world.referee.self_hp = 300;
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 2);
  CHECK(std::get<TacticalMode>(context.intents[0].value) == TacticalMode::kPatrol);
  CHECK(std::get<Point2D>(context.intents[1].value).x == 1.0);
}

void test_invalid_referee_falls_back() {
  DecisionContext context;
  context.world.referee.valid = false;
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", &context);
  auto tree = factory.createTreeFromText(kTree, blackboard);
  context.clear_intents();
  tree.tickOnce();
  CHECK(std::get<TacticalMode>(context.intents[0].value) == TacticalMode::kPatrol);
}

}  // namespace

int main() {
  test_branch_switch();
  test_invalid_referee_falls_back();
  if (g_failures == 0) {
    std::printf("all nodes tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
