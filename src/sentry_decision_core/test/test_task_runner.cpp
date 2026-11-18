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

void test_history_reset_between_matches() {
  const PolicyConfig config = make_config();
  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.self.pose = Point2D{0.0, 0.0, 0.0};
  world.upstream.self_ammo = 10;
  world.upstream.coins = 200;
  world.upstream.game_time_remaining = 420;
  world.upstream.enemy_outpost_hp = 0;

  // 低弹 -> 基地 Heal；满血且没有待领取分钟，立即成功。
  runner.tick(world, TimePoint{});
  const auto heal = runner.take_result();
  CHECK(heal.has_value());
  CHECK(heal->task == "Heal");
  CHECK(heal->status == TaskStatus::kSuccess);

  // 离开比赛中再重新开始：上一局 Heal 的成功不应触发本局 Resource。
  world.upstream.game_status = GameStatus::kPreparation;
  runner.tick(world, TimePoint{} + Duration{1000});
  world.upstream.game_status = GameStatus::kRunning;
  world.upstream.self_ammo = 100;
  runner.tick(world, TimePoint{} + Duration{2000});
  CHECK(runner.task_state() == TaskState::kScan);
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
  test_low_hp_starts_heal();
  test_supply_claim_updates_belief();
  test_history_reset_between_matches();

  if (g_failures == 0) {
    std::printf("all task_runner tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
