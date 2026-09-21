#include <cstdio>
#include <variant>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"
#include "sentry_decision_nodes/common_nodes.hpp"
#include "sentry_decision_nodes/resource_nodes.hpp"

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
      <Sequence name="revive">
        <IfCanFreeResurrect/>
        <RequestFreeRevive/>
      </Sequence>
      <Sequence name="hp">
        <IfLowHp hp_key="resource.hp_exchange_threshold"/>
        <IfCoinsAtLeast coins_key="resource.min_coins"/>
        <RequestHpExchange amount_key="resource.exchange_hp_step"/>
      </Sequence>
      <Sequence name="ammo">
        <IfLowAmmo ammo_key="nav.low_ammo"/>
        <IfCoinsAtLeast coins_key="resource.min_coins"/>
        <RequestAmmoExchange amount_key="resource.exchange_ammo_step"/>
      </Sequence>
    </ReactiveFallback>
  </BehaviorTree>
</root>
)xml";

PolicyConfig make_config() {
  PolicyConfig config;
  config.numbers["resource.hp_exchange_threshold"] = 50.0;
  config.numbers["resource.min_coins"] = 100.0;
  config.numbers["resource.exchange_hp_step"] = 50.0;
  config.numbers["resource.exchange_ammo_step"] = 50.0;
  config.numbers["nav.low_ammo"] = 50.0;
  return config;
}

BT::Tree make_tree(DecisionContext* context) {
  BT::BehaviorTreeFactory factory;
  register_common_nodes(factory);
  register_resource_nodes(factory);
  auto blackboard = BT::Blackboard::create();
  blackboard->set("context", context);
  return factory.createTreeFromText(kTree, blackboard);
}

WorldState make_world() {
  WorldState world;
  world.referee.valid = true;
  world.referee.self_hp = 400;
  world.referee.self_ammo = 100;
  world.referee.coins = 200;
  return world;
}

void test_free_revive() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.referee.info1.can_free_resurrect = true;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(context.intents[0].field == IntentField::kResourceRequest);
  CHECK(std::get<ResourceRequest>(context.intents[0].value).revive);
}

void test_hp_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.referee.self_hp = 30;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  const auto& request = std::get<ResourceRequest>(context.intents[0].value);
  CHECK(request.hp == 50);
  CHECK(request.ammo == 0);
}

void test_ammo_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.referee.self_ammo = 10;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  const auto& request = std::get<ResourceRequest>(context.intents[0].value);
  CHECK(request.ammo == 50);
}

void test_no_coins_no_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.referee.self_hp = 30;
  context.world.referee.coins = 10;  // 不足
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.empty());
}

}  // namespace

int main() {
  test_free_revive();
  test_hp_exchange();
  test_ammo_exchange();
  test_no_coins_no_exchange();
  if (g_failures == 0) {
    std::printf("all resource tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
