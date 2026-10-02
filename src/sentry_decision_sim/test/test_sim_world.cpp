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

// 进入补给区延时：进入当拍（0 秒）不回血，满足延时后的整秒才开始。
void test_supply_heal_ready() {
  CHECK(!supply_heal_ready(0, 1.0));
  CHECK(supply_heal_ready(1, 1.0));
  CHECK(supply_heal_ready(0, 0.0));
  CHECK(!supply_heal_ready(0, 0.5));
  CHECK(supply_heal_ready(1, 0.5));
}

// 仿真效果：扣血 / 扣弹按步长夹到 0，摧毁类置 0，未知效果不生效。
void test_apply_effects() {
  SimEffects effects;
  effects.self_damage = 100;
  effects.self_ammo_consume = 50;
  effects.our_outpost_damage = 300;
  effects.our_base_damage = 500;
  effects.enemy_outpost_damage = 300;
  effects.enemy_base_damage = 500;

  SimWorld world;
  world.self_hp = 400;
  world.self_ammo = 100;
  world.our_outpost_hp = 1500;
  world.base_hp = 5000;
  world.enemy_outpost_hp = 1500;
  world.enemy_base_hp = 5000;

  CHECK(apply_effect(&world, "self_damage", effects).new_value == 300);
  CHECK(world.self_hp == 300);
  CHECK(apply_effect(&world, "self_ammo_consume", effects).new_value == 50);
  world.self_ammo = 20;
  CHECK(apply_effect(&world, "self_ammo_consume", effects).new_value == 0);
  CHECK(apply_effect(&world, "self_death", effects).applied);
  CHECK(world.self_hp == 0);
  CHECK(apply_effect(&world, "our_outpost_damage", effects).new_value == 1200);
  CHECK(apply_effect(&world, "enemy_outpost_destroy", effects).new_value == 0);
  CHECK(world.enemy_outpost_hp == 0);
  CHECK(apply_effect(&world, "our_base_damage", effects).new_value == 4500);
  CHECK(apply_effect(&world, "enemy_base_damage", effects).new_value == 4500);
  CHECK(!apply_effect(&world, "bogus", effects).applied);
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
  // 远程兑换发弹量：未脱战 -> 拒绝；脱战 + 金币足 -> 确认成功但 6s 后才生效。
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
    CHECK(world.coins == 850);    // 金币立即扣除
    CHECK(world.self_ammo == 0);  // 发弹量延迟
    CHECK(world.pending_remote.size() == 1);
    CHECK(step_pending_remote(&world, 5999, 400).ammo_delivered == 0);
    CHECK(world.self_ammo == 0);
    CHECK(step_pending_remote(&world, 1, 400).ammo_delivered == 100);
    CHECK(world.self_ammo == 100);
    CHECK(world.pending_remote.empty());
  }
  // 未知动作 -> 拒绝。
  {
    SimWorld world;
    CHECK(!execute_action(&world, make_action(DecisionActionKind::kNone, 0), 400).accepted);
  }
}

// 补给区免费发弹量（规则 5.3.2）：每满 1 分钟累积 100，占领时领取。
void test_supply_ammo() {
  SimWorld world;
  world.self_ammo = 0;
  // 未占领：不增加，已进行 90s（应累积 1 分钟的份额但不发放）。
  CHECK(claim_supply_ammo(&world, 90, false) == 0);
  CHECK(world.self_ammo == 0);
  // 占领：领取 1 分钟。
  CHECK(claim_supply_ammo(&world, 90, true) == 100);
  CHECK(world.self_ammo == 100);
  // 同一分钟不重复领取。
  CHECK(claim_supply_ammo(&world, 119, true) == 0);
  // 离开期间继续累积：到 390s 时共 6 分钟，已领 1 分钟 -> 再领 500（规则示例）。
  CHECK(claim_supply_ammo(&world, 390, true) == 500);
  CHECK(world.self_ammo == 600);
  // 同一分钟内再次调用不重复领取。
  CHECK(claim_supply_ammo(&world, 419, true) == 0);
  CHECK(world.self_ammo == 600);
  // 跨过 420s 边界再累积 1 分钟。
  CHECK(claim_supply_ammo(&world, 420, true) == 100);
  CHECK(world.self_ammo == 700);

  // 规则 5.3.2 示例：从未占领，剩余 30s（已进行 390s）时才进补给区 -> 一次 +600。
  SimWorld fresh;
  CHECK(claim_supply_ammo(&fresh, 390, true) == 600);
  CHECK(fresh.self_ammo == 600);
}

// 远程兑换血量延迟：6s 后 +60% 上限；期间战亡则作废且金币不返还。
void test_remote_hp_delay() {
  SimWorld world;
  world.coins = 500;
  world.self_hp = 100;
  world.sentry_info_2 = 1;
  world.game_time_remaining = 420;
  const ActionOutcome out =
      execute_action(&world, make_action(DecisionActionKind::kRemoteHpExchange, 1), 400);
  CHECK(out.accepted);
  const int coins_after = world.coins;
  CHECK(coins_after == 450);  // 50 + ROUNDUP((420-420)/60*20) = 50
  CHECK(world.self_hp == 100);
  const RemoteStepResult step = step_pending_remote(&world, 6000, 400);
  CHECK(step.hp_delivered == 240);  // 60% * 400
  CHECK(world.self_hp == 340);
  CHECK(world.coins == coins_after);
}

// 远程兑换血量在 6s 内战亡 -> 作废，金币不返还。
void test_remote_hp_void_on_death() {
  SimWorld world;
  world.coins = 500;
  world.self_hp = 0;  // 已战亡
  world.sentry_info_2 = 1;
  world.game_time_remaining = 420;
  CHECK(
      execute_action(&world, make_action(DecisionActionKind::kRemoteHpExchange, 1), 400).accepted);
  const RemoteStepResult step = step_pending_remote(&world, 6000, 400);
  CHECK(step.voided == 1);
  CHECK(step.hp_delivered == 0);
  CHECK(world.self_hp == 0);
}

}  // namespace

int main() {
  test_exchange_ammo();
  test_exchange_hp();
  test_supply_heal();
  test_supply_heal_ready();
  test_apply_effects();
  test_occupancy();
  test_supply_ammo();
  test_remote_hp_delay();
  test_remote_hp_void_on_death();
  test_execute_action_preconditions();
  if (g_failures == 0) {
    std::printf("all sim world tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
