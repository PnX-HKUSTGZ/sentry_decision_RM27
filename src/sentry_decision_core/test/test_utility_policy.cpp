#include <cstdio>
#include <string>

#include "sentry_decision_core/utility_task_policy.hpp"

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

PolicyConfig make_config() {
  PolicyConfig config;
  config.numbers["self.max_hp"] = 400.0;
  config.numbers["nav.low_ammo"] = 50.0;
  config.numbers["match.duration_s"] = 420.0;
  config.points["own_base_supply"] = Point2D{0.0, 0.0, 0.0};
  config.point_radius["own_base_supply"] = 0.5;
  config.points["own_outpost_buff"] = Point2D{2.0, 0.0, 0.0};
  config.point_radius["own_outpost_buff"] = 0.5;
  config.points["enemy_outpost_buff"] = Point2D{-2.0, 0.0, 0.0};
  config.point_radius["enemy_outpost_buff"] = 0.5;
  config.points["own_outpost_side"] = Point2D{2.0, 2.0, 0.0};
  config.point_radius["own_outpost_side"] = 0.5;
  config.points["own_outpost_opposite"] = Point2D{2.0, -2.0, 0.0};
  config.point_radius["own_outpost_opposite"] = 0.5;
  config.points["own_outpost_front"] = Point2D{1.0, 0.0, 0.0};
  config.points["enemy_outpost_side"] = Point2D{-2.0, 2.0, 0.0};
  config.point_radius["enemy_outpost_side"] = 0.5;
  config.points["enemy_outpost_opposite"] = Point2D{-2.0, -2.0, 0.0};
  config.point_radius["enemy_outpost_opposite"] = 0.5;
  config.points["enemy_outpost_front"] = Point2D{-1.0, 0.0, 0.0};
  return config;
}

WorldState make_world(double x = 0.0, double y = 0.0) {
  WorldState world;
  world.self.valid = true;
  world.self.pose = Point2D{x, y, 0.0};
  world.upstream.valid = true;
  world.upstream.game_status = GameStatus::kRunning;
  world.upstream.game_time_remaining = 240;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 200;
  world.upstream.coins = 500;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_owner = OutpostOwner::kOwn;
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  world.upstream.enemy_outpost_owner = OutpostOwner::kEnemy;
  return world;
}

std::string target_of(const TaskPlan& plan) {
  if (!plan.call.has_value()) {
    return {};
  }
  if (const auto* nav = std::get_if<NavigateParams>(&plan.call->params)) {
    return nav->target;
  }
  if (const auto* heal = std::get_if<HealParams>(&plan.call->params)) {
    return heal->heal_point;
  }
  return {};
}

void test_dead_respawns() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_hp = 0;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Respawn");
}

void test_own_risk_does_not_overextend() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 2.0);
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.our_outpost_trend.under_attack = true;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task != "Outpost");
  if (plan.call->task == "Navigate") {
    const std::string target = target_of(plan);
    CHECK(target.rfind("own_", 0) == 0);
  }
}

void test_own_risk_overrides_enemy_attack_from_far() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, 2.0);  // 敌方高地
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.our_outpost_trend.under_attack = true;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task != "Outpost");
  if (plan.call->task == "Navigate") {
    CHECK(target_of(plan).rfind("own_", 0) == 0);
  }
}

void test_own_highland_does_not_hop_to_other_highland() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 2.0);  // 己方 side
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.our_outpost_trend.under_attack = true;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(!(plan.call->task == "Navigate" &&
          target_of(plan).rfind("own_outpost_", 0) == 0));
}

void test_invalid_own_buff_moves_to_highland() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 0.0);  // 己方 buff
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.our_outpost_trend.under_attack = true;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Navigate");
  const std::string target = target_of(plan);
  CHECK(target == "own_outpost_side" || target == "own_outpost_opposite");
}

void test_active_heal_is_not_preempted_before_full() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, -2.0);
  world.upstream.self_hp = 280;
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.our_outpost_trend.under_attack = true;

  TaskSnapshot heal;
  heal.task = "Heal";
  heal.call = TaskCall{"Heal", HealParams{"own_base_supply", true}, Duration{0}};
  heal.status = TaskStatus::kRunning;

  const TaskPlan plan = policy.decide(world, heal, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(target_of(plan) == "own_base_supply");
}

void test_low_ammo_returns_to_base() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, -2.0);  // 敌方 opposite
  world.upstream.self_ammo = 10;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Navigate");
  CHECK(target_of(plan) == "own_base_supply");
}

void test_low_hp_after_respawn_heals_at_base() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, -2.0);
  world.upstream.self_hp = 40;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(target_of(plan) == "own_base_supply");
}

void test_destroyed_own_buff_low_hp_heals_at_base() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 0.0);  // 己方 buff
  world.upstream.self_hp = 40;
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  world.upstream.our_outpost_hp = 0;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(target_of(plan) == "own_base_supply");
}

void test_full_own_outpost_under_attack_stays_buff() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 0.0);  // 己方 buff
  world.our_outpost_trend.under_attack = true;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan" || plan.call->task == "Heal");
  if (plan.call->task == "Heal") {
    CHECK(target_of(plan) == "own_outpost_buff");
  }
}

void test_both_full_defends_enemy_half() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, 0.0);
  world.upstream.enemy_outpost_control = OutpostControl::kOwnFull;
  world.upstream.enemy_outpost_owner = OutpostOwner::kOwn;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan" || plan.call->task == "Navigate");
  if (plan.call->task == "Navigate") {
    CHECK(target_of(plan).rfind("enemy_", 0) == 0);
  }
}

void test_enemy_outpost_attack_from_highland() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-2.0, 2.0);

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Outpost");
  CHECK(std::get<OutpostParams>(plan.call->params).target_side == Side::kEnemy);
}

void test_front_prefers_highland_when_capture_needed() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-1.0, 0.0);
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Navigate");
  const std::string target = target_of(plan);
  CHECK(target == "enemy_outpost_side" || target == "enemy_outpost_opposite");
}

void test_front_finishes_low_hp_outpost() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-1.0, 0.0);
  world.upstream.enemy_outpost_hp = 100;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Outpost");
}

void test_destroyed_enemy_outpost_is_contested() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-1.0, 0.0);  // 敌方 front
  world.upstream.enemy_outpost_hp = 0;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  world.upstream.enemy_outpost_owner = OutpostOwner::kEnemy;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Navigate");
  const std::string target = target_of(plan);
  CHECK(target == "enemy_outpost_side" || target == "enemy_outpost_opposite");
}

void test_full_hp_at_base_does_not_select_empty_heal() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(0.0, 0.0);
  world.upstream.self_ammo = 10;
  world.upstream.game_time_remaining = 420;  // 无免费批次
  world.supply_claimed_minutes = 0;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task != "Heal");
}

void test_no_navigate_churn_at_destination() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(2.0, 0.0);  // 己方前哨站 buff

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(!(plan.call->task == "Navigate" &&
          target_of(plan) == "own_outpost_buff"));
}

void test_owned_outpost_is_not_attacked() {
  const PolicyConfig config = make_config();
  UtilityTaskPolicy policy(config);
  WorldState world = make_world(-1.0, 0.0);
  world.upstream.enemy_outpost_control = OutpostControl::kOwnTemporary;
  world.upstream.enemy_outpost_owner = OutpostOwner::kOwn;

  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task != "Outpost");
}

}  // namespace

int main() {
  test_dead_respawns();
  test_own_risk_does_not_overextend();
  test_own_risk_overrides_enemy_attack_from_far();
  test_own_highland_does_not_hop_to_other_highland();
  test_invalid_own_buff_moves_to_highland();
  test_active_heal_is_not_preempted_before_full();
  test_low_ammo_returns_to_base();
  test_low_hp_after_respawn_heals_at_base();
  test_destroyed_own_buff_low_hp_heals_at_base();
  test_full_own_outpost_under_attack_stays_buff();
  test_both_full_defends_enemy_half();
  test_enemy_outpost_attack_from_highland();
  test_front_prefers_highland_when_capture_needed();
  test_front_finishes_low_hp_outpost();
  test_destroyed_enemy_outpost_is_contested();
  test_full_hp_at_base_does_not_select_empty_heal();
  test_no_navigate_churn_at_destination();
  test_owned_outpost_is_not_attacked();

  if (g_failures == 0) {
    std::printf("all utility policy tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
