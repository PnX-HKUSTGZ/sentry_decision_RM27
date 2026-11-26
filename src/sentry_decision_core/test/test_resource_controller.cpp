#include <cstdio>

#include "sentry_decision_core/resource_controller.hpp"
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

PolicyConfig make_config(double dwell_s = 0.0) {
  PolicyConfig config;
  config.numbers["resource.low_ammo"] = 10.0;
  config.numbers["resource.batch"] = 100.0;
  config.numbers["resource.reserve_coins"] = 50.0;
  config.numbers["resource.dwell_s"] = dwell_s;
  config.numbers["resource.retry_s"] = 0.0;
  config.points["own_base_supply"] = Point2D{0.0, 0.0, 0.0};
  config.point_radius["own_base_supply"] = 0.5;
  config.points["own_outpost_buff"] = Point2D{5.0, 0.0, 0.0};
  config.point_radius["own_outpost_buff"] = 0.5;
  config.points["enemy_outpost_buff"] = Point2D{-5.0, 0.0, 0.0};
  config.point_radius["enemy_outpost_buff"] = 0.5;
  return config;
}

WorldState make_world(double x = 0.0, double y = 0.0) {
  WorldState world;
  world.self.valid = true;
  world.self.pose = Point2D{x, y, 0.0};
  world.upstream.valid = true;
  world.upstream.game_status = GameStatus::kRunning;
  world.upstream.game_time_remaining = 420;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 0;
  world.upstream.coins = 200;
  world.upstream.event.supply_zone_occupied = true;
  return world;
}

void test_base_starts_and_finishes() {
  const PolicyConfig config = make_config();
  ResourceController controller(config);
  WorldState world = make_world();
  const TimePoint now{};

  controller.tick(world, now);
  const auto action = controller.action();
  CHECK(action.has_value());
  CHECK(action->kind == DecisionActionKind::kAmmoExchange);
  CHECK(action->target == 100);
  CHECK(controller.snapshot().has_value());

  world.upstream.info1.ammo_exchanged_local = 100;
  controller.tick(world, now + Duration{1000});
  CHECK(!controller.action().has_value());
  CHECK(!controller.snapshot().has_value());

  const auto result = controller.take_result();
  CHECK(result.has_value());
  CHECK(result->task == "Resource");
  CHECK(result->status == TaskStatus::kSuccess);
}

void test_respects_low_ammo_and_reserve() {
  const PolicyConfig config = make_config();
  ResourceController controller(config);
  WorldState world = make_world();
  world.upstream.self_ammo = 20;
  controller.tick(world, TimePoint{});
  CHECK(!controller.action().has_value());

  world.upstream.self_ammo = 0;
  world.upstream.coins = 120;
  controller.tick(world, TimePoint{});
  const auto action = controller.action();
  CHECK(action.has_value());
  CHECK(action->target == 70);  // 120 - 50 reserve = 70
}

void test_base_waits_for_free_ammo_before_buying() {
  PolicyConfig config = make_config();
  config.numbers["match.duration_s"] = 420.0;
  ResourceController controller(config);
  WorldState world = make_world();
  world.upstream.game_time_remaining = 330;  // 已进行 90s，有 1 个免费批次
  world.supply_claimed_minutes = 0;
  controller.tick(world, TimePoint{});
  CHECK(!controller.action().has_value());

  world.supply_claimed_minutes = 1;
  controller.tick(world, TimePoint{});
  CHECK(controller.action().has_value());
}

void test_requires_dwell() {
  const PolicyConfig config = make_config(0.5);
  ResourceController controller(config);
  WorldState world = make_world();
  const TimePoint now{};

  controller.tick(world, now);
  CHECK(!controller.action().has_value());
  controller.tick(world, now + Duration{400});
  CHECK(!controller.action().has_value());
  controller.tick(world, now + Duration{600});
  CHECK(controller.action().has_value());
}

void test_outpost_buff_requires_alive_and_full_control() {
  const PolicyConfig config = make_config();

  ResourceController own(config);
  WorldState world = make_world(5.0, 0.0);
  world.upstream.event.supply_zone_occupied = false;
  world.upstream.our_outpost_control = OutpostControl::kOwnFull;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.event.our_outpost_buff_status = 1;
  own.tick(world, TimePoint{});
  CHECK(own.action().has_value());

  ResourceController destroyed(config);
  world.upstream.our_outpost_hp = 0;
  destroyed.tick(world, TimePoint{});
  CHECK(!destroyed.action().has_value());

  ResourceController enemy(config);
  world = make_world(-5.0, 0.0);
  world.upstream.event.supply_zone_occupied = false;
  world.upstream.enemy_outpost_control = OutpostControl::kOwnFull;
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.event.enemy_outpost_buff_status = 1;
  enemy.tick(world, TimePoint{});
  CHECK(enemy.action().has_value());
}

void test_stops_when_dead_or_not_running() {
  const PolicyConfig config = make_config();
  ResourceController controller(config);
  WorldState world = make_world();
  world.upstream.self_hp = 0;
  controller.tick(world, TimePoint{});
  CHECK(!controller.action().has_value());

  world.upstream.self_hp = 400;
  world.upstream.game_status = GameStatus::kPreparation;
  controller.tick(world, TimePoint{});
  CHECK(!controller.action().has_value());
}

void test_task_runner_keeps_main_scan_and_buys_in_background() {
  PolicyConfig config = make_config();
  config.numbers["nav.retreat_hp"] = 50.0;
  config.numbers["nav.low_ammo"] = 20.0;
  config.numbers["match.duration_s"] = 420.0;
  config.numbers["resource.low_ammo"] = 80.0;

  TaskRunner runner(config, 400, Duration{420000});
  WorldState world = make_world();
  world.upstream.self_ammo = 10;
  world.upstream.game_time_remaining = 420;  // 无免费发弹批次
  world.upstream.enemy_outpost_hp = 0;       // 没有可打目标，主栏应 Scan
  world.upstream.event.supply_zone_occupied = true;

  runner.tick(world, TimePoint{});
  CHECK(runner.task_state() == TaskState::kScan);
  const auto action = runner.resource_action();
  CHECK(action.has_value());
  CHECK(action->kind == DecisionActionKind::kAmmoExchange);
  CHECK(runner.resource_snapshot().has_value());
}

}  // namespace

int main() {
  test_base_starts_and_finishes();
  test_respects_low_ammo_and_reserve();
  test_base_waits_for_free_ammo_before_buying();
  test_requires_dwell();
  test_outpost_buff_requires_alive_and_full_control();
  test_stops_when_dead_or_not_running();
  test_task_runner_keeps_main_scan_and_buys_in_background();

  if (g_failures == 0) {
    std::printf("all resource_controller tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
