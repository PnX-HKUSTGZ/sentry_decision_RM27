#include <cstdio>

#include "sentry_decision_core/task_runner.hpp"

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
  config.points["enemy_outpost_front"] = Point2D{5.0, 0.0, 0.0};
  config.point_radius["enemy_outpost_front"] = 0.2;
  return config;
}

WorldState make_world() {
  WorldState world;
  world.stamp = TimePoint{};
  world.self.valid = true;
  world.self.pose = Point2D{0.0, 0.0, 0.0};
  world.nav.valid = true;
  world.upstream.valid = true;
  world.upstream.game_status = GameStatus::kRunning;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 100;
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  world.upstream.enemy_outpost_owner = OutpostOwner::kEnemy;
  return world;
}

void test_navigate_then_success() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  const TimePoint now{};

  runner.tick(world, now);
  CHECK(runner.task_state() == TaskState::kNavigate);
  CHECK(runner.nav_goal().has_value());
  CHECK(runner.nav_goal()->x == 5.0);

  world.nav.reached = true;
  world.self.pose = Point2D{5.0, 0.0, 0.0};
  runner.tick(world, now + Duration{1000});

  const auto result = runner.take_result();
  CHECK(result.has_value());
  CHECK(result->task == "Navigate");
  CHECK(result->status == TaskStatus::kSuccess);
}

void test_dead_starts_respawn() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.upstream.self_hp = 0;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kRespawn);
}

void test_supply_claim_updates_belief() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.self.pose = Point2D{0.0, 0.0, 0.0};
  world.upstream.self_ammo = 10;
  world.upstream.game_time_remaining = 330;  // elapsed 90 s -> 1 minute
  world.upstream.event.supply_zone_occupied = true;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kHeal);

  // 补给区自动发放 100 发，且本地兑换值未变化 -> 认为第 1 分钟批次已领取。
  world.upstream.self_ammo = 110;
  runner.tick(world, TimePoint{} + Duration{1000});

  const auto result = runner.take_result();
  CHECK(result.has_value());
  CHECK(result->task == "Heal");
  CHECK(result->status == TaskStatus::kSuccess);
}

void test_heal_then_scan_without_main_resource() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  // 低血触发 Heal；Heal 成功后主栏回到 Scan，买弹交给独立 Resource 辅助栏。
  world.self.pose = Point2D{0.0, 0.0, 0.0};
  world.upstream.self_hp = 30;
  world.upstream.self_ammo = 100;
  world.upstream.coins = 200;
  world.upstream.game_time_remaining = 420;
  world.upstream.enemy_outpost_hp = 0;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kHeal);
  world.upstream.self_hp = 400;
  runner.tick(world, TimePoint{} + Duration{500});
  const auto heal = runner.take_result();
  CHECK(heal.has_value());
  CHECK(heal->task == "Heal");
  CHECK(heal->status == TaskStatus::kSuccess);

  // 离开比赛中再重新开始：不会出现主栏 Resource。
  world.upstream.game_status = GameStatus::kPreparation;
  runner.tick(world, TimePoint{} + Duration{1000});
  world.upstream.game_status = GameStatus::kRunning;
  runner.tick(world, TimePoint{} + Duration{2000});
  CHECK(runner.task_state() == TaskState::kScan);
}

void test_utility_flag_selects_outpost_at_highland() {
  PolicyConfig config = make_config();
  config.flags["strategy.use_utility"] = true;
  config.points["enemy_outpost_side"] = Point2D{5.0, 2.0, 0.0};
  config.point_radius["enemy_outpost_side"] = 0.5;
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.self.pose = Point2D{5.0, 2.0, 0.0};
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  world.upstream.enemy_outpost_owner = OutpostOwner::kEnemy;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kOutpost);
}

void test_low_hp_starts_heal() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.upstream.self_hp = 30;
  world.upstream.enemy_outpost_hp = 0;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kHeal);
}

}  // namespace

int main() {
  test_navigate_then_success();
  test_dead_starts_respawn();
  test_utility_flag_selects_outpost_at_highland();
  test_low_hp_starts_heal();
  test_supply_claim_updates_belief();
  test_heal_then_scan_without_main_resource();

  if (g_failures == 0) {
    std::printf("all task_runner tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
