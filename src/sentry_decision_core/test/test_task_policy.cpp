#include <cstdio>

#include "sentry_decision_core/task_policy.hpp"

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
  config.numbers["nav.retreat_hp"] = 50.0;
  config.numbers["nav.low_ammo"] = 20.0;
  config.numbers["match.duration_s"] = 420.0;
  config.points["own_base_supply"] = Point2D{0.0, 0.0, 0.0};
  config.point_radius["own_base_supply"] = 0.5;
  config.points["own_outpost_buff"] = Point2D{3.0, 3.0, 0.0};
  config.point_radius["own_outpost_buff"] = 0.5;
  config.points["enemy_outpost_buff"] = Point2D{-3.0, 0.0, 0.0};
  config.point_radius["enemy_outpost_buff"] = 0.5;
  config.points["enemy_outpost_front"] = Point2D{5.0, 0.0, 0.0};
  config.point_radius["enemy_outpost_front"] = 0.2;
  return config;
}

WorldState make_world() {
  WorldState world;
  world.stamp = TimePoint{};
  world.self.valid = true;
  world.self.pose = Point2D{0.0, 0.0, 0.0};
  world.upstream.valid = true;
  world.upstream.game_status = GameStatus::kRunning;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 100;
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  world.upstream.enemy_outpost_owner = OutpostOwner::kEnemy;
  return world;
}

void test_stopped_returns_empty_plan() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.game_status = GameStatus::kPreparation;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(!plan.call.has_value());
}

void test_dead_selects_respawn() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_hp = 0;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Respawn");
}

void test_low_hp_selects_heal() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_hp = 50;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(std::get<HealParams>(plan.call->params).heal_point == "own_base_supply");
}

void test_low_hp_on_own_highland_heals_at_own_outpost_buff() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{3.0, 3.0, 0.0};
  world.upstream.self_hp = 50;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_owner = OutpostOwner::kOwn;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(std::get<HealParams>(plan.call->params).heal_point == "own_outpost_buff");
}

void test_low_hp_with_low_ammo_heals_at_base() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{3.0, 3.0, 0.0};
  world.upstream.self_hp = 50;
  world.upstream.self_ammo = 20;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_owner = OutpostOwner::kOwn;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(std::get<HealParams>(plan.call->params).heal_point == "own_base_supply");
}

void test_low_hp_does_not_use_destroyed_outpost_buff() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{3.0, 3.0, 0.0};
  world.upstream.self_hp = 50;
  world.upstream.our_outpost_hp = 0;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_owner = OutpostOwner::kOwn;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(std::get<HealParams>(plan.call->params).heal_point == "own_base_supply");
}

void test_active_heal_keeps_its_point_when_passing_outpost_buff() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{3.0, 3.0, 0.0};
  world.upstream.self_hp = 50;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_owner = OutpostOwner::kOwn;

  TaskSnapshot heal;
  heal.task = "Heal";
  heal.call = TaskCall{"Heal", HealParams{"own_base_supply", true}, Duration{0}};
  heal.status = TaskStatus::kRunning;

  const TaskPlan plan = policy.decide(world, heal, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
  CHECK(std::get<HealParams>(plan.call->params).heal_point == "own_base_supply");
}

void test_low_ammo_far_from_base_selects_heal() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{5.0, 0.0, 0.0};
  world.upstream.self_ammo = 20;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
}

void test_low_ammo_at_base_does_not_use_main_resource() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_ammo = 20;
  world.upstream.coins = 200;
  world.upstream.game_time_remaining = 420;  // 无免费发弹批次
  world.upstream.enemy_outpost_hp = 0;       // 让主栏落到 Scan
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan");
}

void test_low_ammo_at_base_with_free_batch_selects_heal() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_ammo = 20;
  world.upstream.coins = 0;
  world.upstream.game_time_remaining = 300;  // 已进行 120s，2 个免费批次
  world.supply_claimed_minutes = 0;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Heal");
}

void test_low_ammo_at_base_without_ammo_source_does_not_loop_heal() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.self_ammo = 20;
  world.upstream.coins = 0;
  world.upstream.enemy_outpost_hp = 0;
  world.upstream.game_time_remaining = 360;  // 已进行 60s
  world.supply_claimed_minutes = 1;          // 免费批次已领取
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan");
}

void test_heal_success_at_base_does_not_use_main_resource() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.coins = 200;
  world.upstream.enemy_outpost_hp = 0;
  HistoryView history;
  history.previous_task = TaskResult{};
  history.previous_task->task = "Heal";
  history.previous_task->status = TaskStatus::kSuccess;
  const TaskPlan plan = policy.decide(world, std::nullopt, history);
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan");
}

void test_scan_does_not_block_new_outpost_target() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{5.0, 0.0, 0.0};

  TaskSnapshot scan;
  scan.task = "Scan";
  scan.call = TaskCall{"Scan", ScanParams{}, Duration{0}};
  scan.status = TaskStatus::kRunning;

  const TaskPlan plan = policy.decide(world, scan, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Outpost");
}

void test_enemy_outpost_far_selects_navigate() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Navigate");
  CHECK(std::get<NavigateParams>(plan.call->params).target == "enemy_outpost_front");
  CHECK(plan.call->timeout > Duration{0});
}

void test_enemy_outpost_near_selects_outpost() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.self.pose = Point2D{5.0, 0.0, 0.0};
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Outpost");
  CHECK(std::get<OutpostParams>(plan.call->params).target_side == Side::kEnemy);
}

void test_no_outpost_selects_scan() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.enemy_outpost_hp = 0;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan");
}

void test_owned_outpost_selects_scan() {
  const PolicyConfig config = make_config();
  RuleBasedTaskPolicy policy(config);
  WorldState world = make_world();
  world.upstream.enemy_outpost_control = OutpostControl::kOwnFull;
  const TaskPlan plan = policy.decide(world, std::nullopt, HistoryView{});
  CHECK(plan.call.has_value());
  CHECK(plan.call->task == "Scan");
}

}  // namespace

int main() {
  test_stopped_returns_empty_plan();
  test_dead_selects_respawn();
  test_low_hp_selects_heal();
  test_low_hp_on_own_highland_heals_at_own_outpost_buff();
  test_low_hp_with_low_ammo_heals_at_base();
  test_low_hp_does_not_use_destroyed_outpost_buff();
  test_active_heal_keeps_its_point_when_passing_outpost_buff();
  test_low_ammo_far_from_base_selects_heal();
  test_low_ammo_at_base_does_not_use_main_resource();
  test_low_ammo_at_base_with_free_batch_selects_heal();
  test_low_ammo_at_base_without_ammo_source_does_not_loop_heal();
  test_heal_success_at_base_does_not_use_main_resource();
  test_scan_does_not_block_new_outpost_target();
  test_enemy_outpost_far_selects_navigate();
  test_enemy_outpost_near_selects_outpost();
  test_no_outpost_selects_scan();
  test_owned_outpost_selects_scan();

  if (g_failures == 0) {
    std::printf("all task_policy tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
