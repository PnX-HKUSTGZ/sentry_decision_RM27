#include <cstdio>

#include "sentry_decision_core/referee_protocol.hpp"
#include "sentry_decision_sim/sim_world.hpp"

using namespace sentry_decision;
using namespace sentry_decision_sim;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// 本地兑换发弹量：10 金币/10 发，金币不足 / 数量非法时拒绝。
void test_exchange_ammo() {
  SimWorld world;
  world.coins = 100;
  world.self_ammo = 10;
  const ExchangeResult ok = exchange_ammo(&world, 50);
  CHECK(ok.accepted);
  CHECK(ok.amount == 50);
  CHECK(ok.coin_cost == 50);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  const ExchangeResult poor = exchange_ammo(&world, 80);
  CHECK(!poor.accepted);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  const ExchangeResult zero = exchange_ammo(&world, 0);
  CHECK(!zero.accepted);
}

// 本地兑换血量：1 金币/1 点，受上限约束；满血或金币不足拒绝。
void test_exchange_hp() {
  SimWorld world;
  world.coins = 100;
  world.self_hp = 100;
  const ExchangeResult ok = exchange_hp(&world, 50, 400);
  CHECK(ok.accepted);
  CHECK(ok.amount == 50);
  CHECK(world.self_hp == 150);
  CHECK(world.coins == 50);

  world.coins = 1000;
  const ExchangeResult capped = exchange_hp(&world, 500, 400);
  CHECK(capped.accepted);
  CHECK(world.self_hp == 400);
  CHECK(capped.amount == 250);

  const ExchangeResult full = exchange_hp(&world, 50, 400);
  CHECK(!full.accepted);
  CHECK(world.self_hp == 400);
}

// 补给区回血：按上限血量比例，向上取整，且不超过上限。
void test_supply_heal() {
  SimWorld world;
  world.self_hp = 100;
  CHECK(supply_heal(&world, 400, 0.10) == 40);
  CHECK(world.self_hp == 140);
  CHECK(supply_heal(&world, 400, 0.25) == 100);
  CHECK(world.self_hp == 240);
  world.self_hp = 395;
  CHECK(supply_heal(&world, 400, 0.10) == 5);
  CHECK(world.self_hp == 400);
  CHECK(supply_heal(&world, 400, 0.10) == 0);
  // 上限 250 时 10% = 25。
  world.self_hp = 0;
  CHECK(supply_heal(&world, 250, 0.10) == 25);
}

// 增益点占领：按位姿判定并写回 event_code 位段。
void test_occupancy() {
  GainZones zones;
  zones.supply = {-6.0, 4.0, 1.5};
  zones.base_buff = {-5.0, 3.0, 0.0};  // 未启用
  zones.our_outpost_buff = {1.1, 1.1, 1.0};
  zones.fort_buff = {0.0, 0.0, 0.0};

  const Occupancy home = evaluate_occupancy(zones, -5.0, 3.0);
  CHECK(home.supply);
  CHECK(!home.base_buff);
  CHECK(home.local_ammo_exchange_point());

  const Occupancy enemy_side = evaluate_occupancy(zones, 4.0, 4.0);
  CHECK(!enemy_side.local_ammo_exchange_point());

  const Occupancy outpost = evaluate_occupancy(zones, 1.1, 1.1);
  CHECK(outpost.our_outpost_buff);
  CHECK(outpost.local_ammo_exchange_point());

  SimWorld world;
  apply_occupancy(&world, home);
  EventCode event = decode_event_code(world.event_code);
  CHECK(event.supply_zone_occupied);
  CHECK(event.local_ammo_exchange_point());

  apply_occupancy(&world, enemy_side);
  event = decode_event_code(world.event_code);
  CHECK(!event.supply_zone_occupied);
  CHECK(!event.local_ammo_exchange_point());
}

DecisionAction make_action(DecisionActionKind kind, int value) {
  DecisionAction action;
  action.kind = kind;
  action.mode = ActionMode::kOneShot;
  action.value = value;
  action.request_id = 1;
  return action;
}

// 裁判侧前置校验：非法动作不改世界。
void test_execute_action_preconditions() {
  // 兑换发弹量：不在增益点 -> 拒绝。
  {
    SimWorld world;
    world.coins = 200;
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kAmmoExchange, 50), 400);
    CHECK(!out.accepted);
    CHECK(out.code == 2);
    CHECK(world.self_ammo == 0);
    CHECK(world.coins == 200);
  }
  // 兑换发弹量：在增益点 -> 成功扣币加弹。
  {
    SimWorld world;
    world.coins = 200;
    GainZones zones;
    zones.supply = {-6.0, 4.0, 1.5};
    apply_occupancy(&world, evaluate_occupancy(zones, -5.0, 3.0));
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kAmmoExchange, 50), 400);
    CHECK(out.accepted);
    CHECK(world.self_ammo == 50);
    CHECK(world.coins == 150);
  }
  // 兑换血量：未脱战 -> 拒绝；脱战 -> 成功。
  {
    SimWorld world;
    world.coins = 200;
    world.self_hp = 100;
    world.sentry_info_2 = 0;
    const ActionOutcome engaged =
        execute_action(&world, make_action(DecisionActionKind::kHpExchange, 50), 400);
    CHECK(!engaged.accepted);
    CHECK(engaged.code == 2);
    CHECK(world.self_hp == 100);
    world.sentry_info_2 = 1;
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kHpExchange, 50), 400);
    CHECK(out.accepted);
    CHECK(world.self_hp == 150);
    CHECK(world.coins == 150);
  }
  // 免费复活：未授权 -> 拒绝；授权 -> 成功。
  {
    SimWorld world;
    CHECK(
        !execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 0), 400).accepted);
    world.sentry_info_1 = (1u << 19);
    CHECK(execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 0), 400).accepted);
  }
  // 立即复活：金币不足 -> 拒绝；足够 -> 成功。
  {
    SimWorld world;
    world.coins = 100;
    world.self_hp = 0;
    world.sentry_info_1 = (1u << 20) | (120u << 21);
    const ActionOutcome poor =
        execute_action(&world, make_action(DecisionActionKind::kInstantResurrect, 0), 400);
    CHECK(!poor.accepted);
    CHECK(poor.code == 3);
    world.coins = 200;
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kInstantResurrect, 0), 400);
    CHECK(out.accepted);
    CHECK(world.self_hp == 400);
    CHECK(world.coins == 80);
  }
  // 远程兑换发弹量：未脱战 -> 拒绝；脱战 + 金币足 -> 成功。
  {
    SimWorld world;
    world.coins = 1000;
    world.sentry_info_2 = 0;
    CHECK(!execute_action(&world, make_action(DecisionActionKind::kRemoteAmmoExchange, 1), 400)
               .accepted);
    world.sentry_info_2 = 1;
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kRemoteAmmoExchange, 1), 400);
    CHECK(out.accepted);
    CHECK(world.self_ammo == 100);
    CHECK(world.coins == 850);
  }
  // 未知动作 -> 拒绝。
  {
    SimWorld world;
    CHECK(!execute_action(&world, make_action(DecisionActionKind::kNone, 0), 400).accepted);
  }
}

}  // namespace

int main() {
  test_exchange_ammo();
  test_exchange_hp();
  test_supply_heal();
  test_occupancy();
  test_execute_action_preconditions();
  if (g_failures == 0) {
    std::printf("all sim world tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
