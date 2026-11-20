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
  EventCode event = decode_event_code(world.event_code, 0, TeamColor::kRed);
  CHECK(event.supply_zone_occupied);
  CHECK(event.local_ammo_exchange_point());

  apply_occupancy(&world, enemy_side);
  event = decode_event_code(world.event_code, 0, TeamColor::kRed);
  CHECK(!event.supply_zone_occupied);
  CHECK(!event.local_ammo_exchange_point());

  // 敌方前哨增益点：只有己方完全控制时才发送 1，否则按协议发送 4。
  SimWorld outpost_world;
  outpost_world.enemy_outpost_hp = 1000;
  Occupancy enemy_buff;
  enemy_buff.enemy_outpost_buff = true;
  outpost_world.enemy_outpost_control.held_by_us = true;
  outpost_world.enemy_outpost_control.full = true;
  apply_occupancy(&outpost_world, enemy_buff);
  apply_outpost_control_bits(&outpost_world);
  event = decode_event_code(outpost_world.event_code, 0, TeamColor::kRed);
  CHECK(event.enemy_outpost_buff_status == 1);
  CHECK(event.enemy_outpost_control == OutpostControl::kOwnFull);
  CHECK(event.local_ammo_exchange_point());

  outpost_world.enemy_outpost_control.full = false;
  apply_occupancy(&outpost_world, enemy_buff);
  apply_outpost_control_bits(&outpost_world);
  event = decode_event_code(outpost_world.event_code, 0, TeamColor::kRed);
  CHECK(event.enemy_outpost_buff_status == 1);  // 临时控制仍算控制
  CHECK(event.enemy_outpost_control == OutpostControl::kOwnTemporary);
  CHECK(!event.local_ammo_exchange_point());

  outpost_world.enemy_outpost_control.held_by_us = false;
  apply_occupancy(&outpost_world, enemy_buff);
  apply_outpost_control_bits(&outpost_world);
  event = decode_event_code(outpost_world.event_code, 0, TeamColor::kRed);
  CHECK(event.enemy_outpost_buff_status == 4);  // 未控制时不发送增益点状态
  CHECK(event.enemy_outpost_control == OutpostControl::kNotOwned);

  // 前哨站被击毁后，前哨站增益点失效。
  outpost_world.enemy_outpost_hp = 0;
  outpost_world.enemy_outpost_control.held_by_us = true;
  outpost_world.enemy_outpost_control.full = true;
  apply_occupancy(&outpost_world, enemy_buff);
  apply_outpost_control_bits(&outpost_world);
  event = decode_event_code(outpost_world.event_code, 0, TeamColor::kRed);
  CHECK(event.enemy_outpost_buff_status == 4);
  CHECK(!event.local_ammo_exchange_point());

  // 中央高地增益点：控制对应阵地时写 1，未控制时写 3。
  SimWorld highland_world;
  highland_world.enemy_outpost_control.held_by_us = true;
  highland_world.enemy_outpost_control.full = true;
  Occupancy highland;
  highland.our_highland = true;
  highland.enemy_highland = true;
  apply_occupancy(&highland_world, highland);
  event = decode_event_code(highland_world.event_code, 0, TeamColor::kRed);
  CHECK(event.own_central_highland_status == 1);
  CHECK(event.enemy_central_highland_status == 1);

  highland_world.enemy_outpost_control.held_by_us = false;
  apply_occupancy(&highland_world, highland);
  event = decode_event_code(highland_world.event_code, 0, TeamColor::kRed);
  CHECK(event.enemy_central_highland_status == 3);
}

// 2027 前哨控制：两条转化路径 + 己方前哨失守。
void test_outpost_control_paths() {
  // 路径 1：占增益点 5 s -> 临时控制 -> 击毁 -> 完全控制并重建 500 血。
  {
    SimWorld world;
    world.enemy_outpost_hp = 1000;
    CHECK(!world.enemy_outpost_control.held_by_us);
    CHECK(world.enemy_outpost_control.full);

    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 4999);
    CHECK(!world.enemy_outpost_control.held_by_us);
    CHECK(world.enemy_outpost_control.full);
    CHECK(world.enemy_outpost_control.challenge_ms == 4999);

    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 1);
    CHECK(world.enemy_outpost_control.held_by_us);
    CHECK(!world.enemy_outpost_control.full);
    CHECK(world.enemy_outpost_hp == 1000);

    destroy_outpost(&world.enemy_outpost_control, &world.enemy_outpost_hp, true);
    CHECK(world.enemy_outpost_control.held_by_us);
    CHECK(world.enemy_outpost_control.full);
    CHECK(world.enemy_outpost_control.owner_is_us);
    CHECK(world.enemy_outpost_hp == 500);
  }

  // 路径 2：先击毁 -> 对方临时控制 -> 对方离开 5 s -> 我方完全控制并重建。
  {
    SimWorld world;
    world.enemy_outpost_hp = 1000;
    destroy_outpost(&world.enemy_outpost_control, &world.enemy_outpost_hp, true);
    CHECK(!world.enemy_outpost_control.held_by_us);
    CHECK(!world.enemy_outpost_control.full);
    CHECK(world.enemy_outpost_hp == 0);

    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, false, 4999);
    CHECK(!world.enemy_outpost_control.held_by_us);
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, false, 1);
    CHECK(world.enemy_outpost_control.held_by_us);
    CHECK(world.enemy_outpost_control.full);
    CHECK(world.enemy_outpost_control.owner_is_us);
    CHECK(world.enemy_outpost_hp == 500);
  }

  // 己方前哨被击毁：先转临时控制；我方离开 5 s 后对方完全控制并重建。
  {
    SimWorld world;
    world.our_outpost_hp = 1000;
    destroy_outpost(&world.our_outpost_control, &world.our_outpost_hp, false);
    CHECK(world.our_outpost_control.held_by_us);
    CHECK(!world.our_outpost_control.full);
    CHECK(world.our_outpost_hp == 0);

    step_outpost_control(&world.our_outpost_control, &world.our_outpost_hp, false, 5000);
    CHECK(!world.our_outpost_control.held_by_us);
    CHECK(world.our_outpost_control.full);
    CHECK(!world.our_outpost_control.owner_is_us);
    CHECK(world.our_outpost_hp == 500);
  }

  // 对方占点 5 s：己方完全控制 -> 对方临时控制；对方再击毁 -> 对方完全控制。
  {
    SimWorld world;
    world.our_outpost_hp = 1000;
    world.our_outpost_control.enemy_present = true;
    step_outpost_control(&world.our_outpost_control, &world.our_outpost_hp, false, 5000);
    CHECK(!world.our_outpost_control.held_by_us);
    CHECK(!world.our_outpost_control.full);

    destroy_outpost(&world.our_outpost_control, &world.our_outpost_hp, false);
    CHECK(!world.our_outpost_control.held_by_us);
    CHECK(world.our_outpost_control.full);
    CHECK(!world.our_outpost_control.owner_is_us);
    CHECK(world.our_outpost_hp == 500);
  }

  // 同方临时 -> 完全：不重建，保持 0 血（正常流程不应出现，仅验证规则边界）。
  {
    SimWorld world;
    world.our_outpost_hp = 1000;
    world.our_outpost_control.held_by_us = true;
    world.our_outpost_control.full = false;
    world.our_outpost_control.owner_is_us = true;
    destroy_outpost(&world.our_outpost_control, &world.our_outpost_hp, true);
    CHECK(world.our_outpost_control.held_by_us);
    CHECK(world.our_outpost_control.full);
    CHECK(world.our_outpost_control.owner_is_us);
    CHECK(world.our_outpost_hp == 0);
  }

  // 中央高地增益点也可作为前哨阵地的占点输入。
  {
    GainZones zones;
    zones.enemy_highland = {{0.0, 0.0, 0.5}};
    const Occupancy highland = evaluate_occupancy(zones, 0.2, 0.2);
    CHECK(highland.enemy_highland);
    CHECK(!highland.enemy_outpost_buff);

    SimWorld world;
    world.enemy_outpost_hp = 1000;
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp,
                         highland.enemy_highland, 5000);
    CHECK(world.enemy_outpost_control.held_by_us);
    CHECK(!world.enemy_outpost_control.full);
    CHECK(!world.enemy_outpost_control.owner_is_us);
  }

  // 双方同时激活：占领进度暂停；单方离开后进度按同速倒退。
  {
    SimWorld world;
    world.enemy_outpost_hp = 1000;
    world.enemy_outpost_control.enemy_present = true;
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 2500);
    CHECK(world.enemy_outpost_control.challenge_ms == 0);

    world.enemy_outpost_control.enemy_present = false;
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 2500);
    CHECK(world.enemy_outpost_control.challenge_ms == 2500);
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, false, 1000);
    CHECK(world.enemy_outpost_control.challenge_ms == 1500);
    step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 3500);
    CHECK(world.enemy_outpost_control.held_by_us);
    CHECK(!world.enemy_outpost_control.full);
  }
}

void test_temporary_holder_destroy_rebuilds_as_us() {
  SimWorld world;
  world.enemy_outpost_hp = 1000;
  world.enemy_outpost_control.held_by_us = false;
  world.enemy_outpost_control.full = true;
  world.enemy_outpost_control.owner_is_us = false;

  step_outpost_control(&world.enemy_outpost_control, &world.enemy_outpost_hp, true, 5000);
  CHECK(world.enemy_outpost_control.held_by_us);
  CHECK(!world.enemy_outpost_control.full);
  CHECK(!world.enemy_outpost_control.owner_is_us);

  SimEffects effects;
  const EffectResult result = apply_effect(&world, "enemy_outpost_destroy", effects);
  CHECK(result.applied);
  CHECK(world.enemy_outpost_control.held_by_us);
  CHECK(world.enemy_outpost_control.full);
  CHECK(world.enemy_outpost_control.owner_is_us);
  CHECK(world.enemy_outpost_hp == 500);
}

void test_our_temporary_holder_destroy_rebuilds_as_us() {
  SimWorld world;
  world.our_outpost_hp = 1000;
  world.our_outpost_control.held_by_us = false;
  world.our_outpost_control.full = true;
  world.our_outpost_control.owner_is_us = false;

  // 敌方完全控制 -> 我方占点 5 s -> 我方临时控制，归属仍敌方。
  step_outpost_control(&world.our_outpost_control, &world.our_outpost_hp, true, 5000);
  CHECK(world.our_outpost_control.held_by_us);
  CHECK(!world.our_outpost_control.full);
  CHECK(!world.our_outpost_control.owner_is_us);

  // 我方在临时控制下击毁 -> 我方完全控制并重建。
  SimEffects effects;
  const EffectResult result = apply_effect(&world, "our_outpost_destroy", effects);
  CHECK(result.applied);
  CHECK(world.our_outpost_control.held_by_us);
  CHECK(world.our_outpost_control.full);
  CHECK(world.our_outpost_control.owner_is_us);
  CHECK(world.our_outpost_hp == 500);
}

void test_dead_holder_loses_temporary_control_and_rebuilds_enemy() {
  SimWorld world;
  world.our_outpost_control.held_by_us = true;
  world.our_outpost_control.full = false;
  world.our_outpost_control.owner_is_us = true;
  world.our_outpost_hp = 0;
  world.our_outpost_control.enemy_present = false;

  // 阵亡后本拍 our_active=false；临时控制方连续 5 s 不激活。
  step_outpost_control(&world.our_outpost_control, &world.our_outpost_hp, false, 5000);
  CHECK(!world.our_outpost_control.held_by_us);
  CHECK(world.our_outpost_control.full);
  CHECK(!world.our_outpost_control.owner_is_us);
  CHECK(world.our_outpost_hp == 500);
}

void test_dead_robot_does_not_occupy_gain_points() {
  GainZones zones;
  zones.supply = {0.0, 0.0, 1.0};
  zones.our_outpost_buff = {0.0, 0.0, 1.0};
  zones.enemy_highland = {{0.0, 0.0, 1.0}};

  const Occupancy dead = evaluate_occupancy_if_alive(zones, 0.0, 0.0, false);
  CHECK(!dead.supply);
  CHECK(!dead.our_outpost_buff);
  CHECK(!dead.enemy_highland);
  CHECK(!dead.local_ammo_exchange_point());

  const Occupancy alive = evaluate_occupancy_if_alive(zones, 0.0, 0.0, true);
  CHECK(alive.supply);
  CHECK(alive.our_outpost_buff);
  CHECK(alive.enemy_highland);
}

DecisionAction make_action(DecisionActionKind kind, int value) {
  DecisionAction action;
  action.kind = kind;
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
    CHECK(decode_sentry_info1(world.sentry_info_1).ammo_exchanged_local == 50);

    // 同一累计目标重复下发：幂等，不重复扣币 / 加弹。
    const ActionOutcome repeat =
        execute_action(&world, make_action(DecisionActionKind::kAmmoExchange, 50), 400);
    CHECK(repeat.accepted);
    CHECK(world.self_ammo == 50);
    CHECK(world.coins == 150);

    // 累计目标继续增加：只按增量结算。
    const ActionOutcome more =
        execute_action(&world, make_action(DecisionActionKind::kAmmoExchange, 80), 400);
    CHECK(more.accepted);
    CHECK(world.self_ammo == 80);
    CHECK(world.coins == 120);

    // 累计目标回退：非法，不改世界。
    const ActionOutcome lower =
        execute_action(&world, make_action(DecisionActionKind::kAmmoExchange, 30), 400);
    CHECK(!lower.accepted);
    CHECK(lower.code == 4);
    CHECK(world.self_ammo == 80);
  }
  // 免费复活：存活 -> 拒绝；待复活 -> 接受并确认（读条未完成时不改血量）。
  {
    SimWorld world;
    world.self_hp = 100;  // 存活
    CHECK(
        !execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 1), 400).accepted);
    world.self_hp = 0;  // 战亡 -> 裁判推导待复活
    world.game_time_remaining = 420;
    const ActionOutcome confirm =
        execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 1), 400);
    CHECK(confirm.accepted);
    CHECK(world.respawn.pending);
    CHECK(world.respawn.confirmed);
    CHECK(world.self_hp == 0);  // 读条未完成，不复活

    // 电平清零：取消确认。
    CHECK(execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 0), 400).accepted);
    CHECK(!world.respawn.confirmed);
  }
  // 立即复活：金币不足 -> 拒绝；足够 -> 成功。
  {
    SimWorld world;
    world.coins = 100;
    world.self_hp = 0;
    world.sentry_info_1 = (1u << 20) | (120u << 21);
    const ActionOutcome poor =
        execute_action(&world, make_action(DecisionActionKind::kInstantResurrect, 1), 400);
    CHECK(!poor.accepted);
    CHECK(poor.code == 3);
    world.coins = 200;
    const ActionOutcome out =
        execute_action(&world, make_action(DecisionActionKind::kInstantResurrect, 1), 400);
    CHECK(out.accepted);
    CHECK(world.self_hp == 400);
    CHECK(world.coins == 80);
    CHECK(world.respawn.instant_count == 1);
    CHECK(!world.respawn.pending);
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
    CHECK(decode_sentry_info1(world.sentry_info_1).remote_ammo_exchange_count == 1);
    // 同一累计次数重复下发：幂等，不再扣币 / 入队。
    CHECK(execute_action(&world, make_action(DecisionActionKind::kRemoteAmmoExchange, 1), 400)
              .accepted);
    CHECK(world.coins == 850);
    CHECK(world.pending_remote.size() == 1);
    // 一次跳多次：规则要求每次仅 +1，拒绝。
    CHECK(!execute_action(&world, make_action(DecisionActionKind::kRemoteAmmoExchange, 3), 400)
               .accepted);
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

// 复活读条总长度（规则 5.2.2）：10 + round((420-剩余)/10) + 20*累计立即复活次数。
void test_respawn_bar_total() {
  CHECK(respawn_bar_total(420, 0) == 10);
  CHECK(respawn_bar_total(300, 0) == 22);  // 10 + 12
  CHECK(respawn_bar_total(120, 2) == 80);  // 规则示例
  CHECK(respawn_bar_total(500, 0) == 10);  // 赛前剩余 >420 按 0 处理
}

// 战亡后同步：can_free_resurrect 置位、读条总长锁定、热量清零；复活后清位。
void test_sync_respawn_starts_bar() {
  SimWorld world;
  world.self_hp = 0;
  world.game_time_remaining = 300;
  world.current_heat = 200;
  world.sentry_info_1 = 0;
  sync_respawn(&world);
  CHECK(world.respawn.pending);
  CHECK(world.respawn.total_progress == 22);
  CHECK(world.respawn.current_progress == 0);
  CHECK(!world.respawn.confirmed);
  CHECK(world.current_heat == 0);
  CHECK(decode_sentry_info1(world.sentry_info_1).can_free_resurrect);

  world.self_hp = 40;  // 复活后
  sync_respawn(&world);
  CHECK(!world.respawn.pending);
  CHECK(!decode_sentry_info1(world.sentry_info_1).can_free_resurrect);
}

// 确认后读条完成 -> 10% 上限血复活；补给区 / 基地 <4000 时每秒 +4；未确认不复活。
void test_step_respawn_revives_at_ten_percent() {
  SimWorld world;
  world.self_hp = 0;
  world.sentry_info_2 = 1;
  world.game_time_remaining = 420;  // 读条 10s
  CHECK(execute_action(&world, make_action(DecisionActionKind::kFreeResurrect, 1), 400).accepted);

  for (int i = 0; i < 9; ++i) {
    CHECK(!step_respawn(&world, 400, false).revived);
  }
  CHECK(world.self_hp == 0);
  CHECK(world.respawn.current_progress == 9);
  const RespawnStepResult final_step = step_respawn(&world, 400, false);
  CHECK(final_step.revived);
  CHECK(world.self_hp == 40);  // 10% * 400
  CHECK(final_step.healed == 40);
  CHECK(!world.respawn.pending);

  // 未确认：读条完成也不复活；完成后再确认立即复活。
  SimWorld unconfirmed;
  unconfirmed.self_hp = 0;
  unconfirmed.game_time_remaining = 420;
  sync_respawn(&unconfirmed);
  for (int i = 0; i < 10; ++i) {
    step_respawn(&unconfirmed, 400, false);
  }
  CHECK(unconfirmed.self_hp == 0);
  CHECK(unconfirmed.respawn.pending);
  const ActionOutcome late =
      execute_action(&unconfirmed, make_action(DecisionActionKind::kFreeResurrect, 1), 400);
  CHECK(late.accepted);
  CHECK(unconfirmed.self_hp == 40);

  // 加速：每秒 +4，三次读数即完成。
  SimWorld fast;
  fast.self_hp = 0;
  fast.game_time_remaining = 420;
  sync_respawn(&fast);
  fast.respawn.confirmed = true;
  CHECK(!step_respawn(&fast, 400, true).revived);  // 4
  CHECK(!step_respawn(&fast, 400, true).revived);  // 8
  CHECK(step_respawn(&fast, 400, true).revived);   // 12 -> 夹到 10
  CHECK(fast.self_hp == 40);
}

// 立即复活：本局累计次数 +1，并计入后续免费复活读条。
void test_instant_resurrect_counts() {
  SimWorld world;
  world.coins = 200;
  world.self_hp = 0;
  world.game_time_remaining = 420;
  world.sentry_info_1 = (1u << 20) | (120u << 21);
  const ActionOutcome out =
      execute_action(&world, make_action(DecisionActionKind::kInstantResurrect, 1), 400);
  CHECK(out.accepted);
  CHECK(world.self_hp == 400);
  CHECK(world.coins == 80);
  CHECK(world.respawn.instant_count == 1);
  CHECK(!world.respawn.pending);

  world.self_hp = 0;
  sync_respawn(&world);
  CHECK(world.respawn.total_progress == 30);  // 420 剩余 -> 10 + 20*1
}

}  // namespace

int main() {
  test_exchange_ammo();
  test_exchange_hp();
  test_supply_heal();
  test_supply_heal_ready();
  test_apply_effects();
  test_occupancy();
  test_dead_robot_does_not_occupy_gain_points();
  test_dead_holder_loses_temporary_control_and_rebuilds_enemy();
  test_temporary_holder_destroy_rebuilds_as_us();
  test_our_temporary_holder_destroy_rebuilds_as_us();
  test_outpost_control_paths();
  test_supply_ammo();
  test_remote_hp_delay();
  test_remote_hp_void_on_death();
  test_execute_action_preconditions();
  test_respawn_bar_total();
  test_sync_respawn_starts_bar();
  test_step_respawn_revives_at_ten_percent();
  test_instant_resurrect_counts();
  if (g_failures == 0) {
    std::printf("all sim world tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
