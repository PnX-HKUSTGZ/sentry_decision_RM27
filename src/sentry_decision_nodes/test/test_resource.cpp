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
      <Sequence name="instant">
        <IfCanInstantResurrect/>
        <RequestInstantRevive/>
      </Sequence>
      <Sequence name="free">
        <IfCanFreeResurrect/>
        <RequestFreeRevive/>
      </Sequence>
      <Sequence name="remote_hp">
        <IfLowHp hp_key="resource.hp_exchange_threshold"/>
        <IfDisengaged/>
        <IfCoinsAtLeast coins_key="resource.min_coins"/>
        <RequestRemoteHpExchange times_key="resource.remote_hp_times"/>
      </Sequence>
      <Sequence name="local_ammo">
        <IfLowAmmo ammo_key="nav.low_ammo"/>
        <IfOccupyingGainPoint/>
        <IfCoinsAtLeast coins_key="resource.min_coins"/>
        <RequestAmmoExchange amount_key="resource.exchange_ammo_step"/>
      </Sequence>
      <Sequence name="remote_ammo">
        <IfLowAmmo ammo_key="nav.low_ammo"/>
        <IfDisengaged/>
        <IfNotOccupyingGainPoint/>
        <IfCoinsAtLeast coins_key="resource.remote_ammo_min_coins"/>
        <RequestRemoteAmmoExchange times_key="resource.remote_ammo_times"/>
      </Sequence>
    </ReactiveFallback>
  </BehaviorTree>
</root>
)xml";

PolicyConfig make_config() {
  PolicyConfig config;
  config.numbers["resource.hp_exchange_threshold"] = 50.0;
  config.numbers["resource.min_coins"] = 100.0;
  config.numbers["resource.exchange_ammo_step"] = 50.0;
  config.numbers["resource.remote_hp_times"] = 1.0;
  config.numbers["resource.remote_ammo_times"] = 1.0;
  config.numbers["resource.remote_ammo_min_coins"] = 150.0;
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
  world.upstream.valid = true;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 100;
  world.upstream.coins = 200;
  world.upstream.event.supply_zone_occupied = true;  // 默认在可本地兑换的增益点
  world.upstream.info2.disengaged = true;            // 默认脱战
  return world;
}

void test_free_revive() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.info1.can_free_resurrect = true;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(context.intents[0].field == IntentField::kResourceRequest);
  CHECK(std::get<ResourceRequest>(context.intents[0].value).revive);
}

// 血量兑换按规则只走远程：脱战 + 金币够 -> remote_hp 次数。
void test_remote_hp_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_hp = 30;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  const auto& request = std::get<ResourceRequest>(context.intents[0].value);
  CHECK(request.remote_hp == 1);
  CHECK(request.hp == 0);
  CHECK(request.ammo == 0);
}

// 立即复活优先，且金币不足时不请求。
void test_instant_revive() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_hp = 0;
  context.world.upstream.info1.can_instant_resurrect = true;
  context.world.upstream.info1.instant_resurrect_cost = 120;
  context.world.upstream.coins = 200;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(std::get<ResourceRequest>(context.intents[0].value).instant_revive);
}

void test_instant_revive_blocked_without_coins() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_hp = 0;
  context.world.upstream.info1.can_instant_resurrect = true;
  context.world.upstream.info1.instant_resurrect_cost = 120;
  context.world.upstream.coins = 50;  // 不足以立即复活 / 远程血量 / 本地兑换
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.empty());
}

void test_local_ammo_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_ammo = 10;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  const auto& request = std::get<ResourceRequest>(context.intents[0].value);
  CHECK(request.ammo == 50);
  CHECK(request.remote_ammo == 0);
}

// 不在增益点且脱战时，改走远程兑换发弹量。
void test_remote_ammo_exchange_without_gain_point() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_ammo = 10;
  context.world.upstream.event = {};
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  const auto& request = std::get<ResourceRequest>(context.intents[0].value);
  CHECK(request.remote_ammo == 1);
  CHECK(request.ammo == 0);
}

// 远程兑换发弹量金币不足（100 < 150）时不请求。
void test_remote_ammo_blocked_when_coins_low() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_ammo = 10;
  context.world.upstream.event = {};
  context.world.upstream.coins = 100;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.empty());
}

void test_no_coins_no_exchange() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_hp = 30;
  context.world.upstream.coins = 10;  // 不足
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.empty());
}

// RMUC 场次的补给区占用走 event bit 0，视为可本地兑换增益点。
void test_ammo_exchange_accepts_supply_bit() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_ammo = 10;
  context.world.upstream.event = {};
  context.world.upstream.event.supply_zone_occupied = true;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.size() == 1);
  CHECK(std::get<ResourceRequest>(context.intents[0].value).ammo == 50);
}

// 前置条件：未脱战时不请求远程兑换血量 / 发弹量。
void test_remote_exchange_blocked_when_engaged() {
  PolicyConfig config = make_config();
  DecisionContext context;
  context.config = &config;
  context.world = make_world();
  context.world.upstream.self_hp = 30;
  context.world.upstream.info2.disengaged = false;
  BT::Tree tree = make_tree(&context);
  context.clear_intents();
  tree.tickOnce();
  CHECK(context.intents.empty());
}

}  // namespace

int main() {
  test_free_revive();
  test_remote_hp_exchange();
  test_instant_revive();
  test_instant_revive_blocked_without_coins();
  test_local_ammo_exchange();
  test_remote_ammo_exchange_without_gain_point();
  test_remote_ammo_blocked_when_coins_low();
  test_no_coins_no_exchange();
  test_ammo_exchange_accepts_supply_bit();
  test_remote_exchange_blocked_when_engaged();
  if (g_failures == 0) {
    std::printf("all resource tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}