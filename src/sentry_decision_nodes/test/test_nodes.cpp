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

PolicyConfig make_config() {
  PolicyConfig config;
  config.points["home"] = Point2D{-2.0, 0.0, 0.0};
  config.points["patrol_a"] = Point2D{1.0, 1.0, 0.0};
  config.numbers["nav.retreat_hp"] = 120.0;
  return config;
}

BT::Tree make_tree(DecisionContext* context) {
  BT::BehaviorTreeFactory factory;
  register_sentry_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", context);
  return factory.createTreeFromText(kTree, blackboard);
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

}  // namespace

int main() {
  test_branch_switch();
  test_invalid_referee_falls_back();
  test_missing_config_key_falls_back();
  if (g_failures == 0) {
    std::printf("all nodes tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
