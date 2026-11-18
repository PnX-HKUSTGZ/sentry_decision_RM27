#include <cstdio>

#include "sentry_decision_core/heal_task.hpp"
#include "sentry_decision_core/resource_task.hpp"

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
  config.points["own_base_supply"] = Point2D{0.0, 0.0, 0.0};
  config.point_radius["own_base_supply"] = 0.5;
  config.points["own_outpost_buff"] = Point2D{5.0, 0.0, 0.0};
  config.point_radius["own_outpost_buff"] = 0.5;
  return config;
}

WorldState make_world(double x, double y, int hp = 400) {
  WorldState world;
  world.self.valid = true;
  world.self.pose = Point2D{x, y, 0.0};
  world.upstream.valid = true;
  world.upstream.self_hp = hp;
  world.upstream.game_time_remaining = 330;  // elapsed 90 s -> 1 minute
  world.nav.valid = true;
  return world;
}

TaskCall heal_call(const char* point, bool claim = true) {
  return TaskCall{"Heal", HealParams{point, claim}, Duration{0}};
}

void test_heal_navigates_and_waits_for_ammo() {
  const PolicyConfig config = make_config();
  HealTask task(config, 400, Duration{420000});

  const TimePoint now{};
  task.start(heal_call("own_base_supply"), now);

  WorldState far = make_world(2.0, 0.0);
  TaskOutput moving = task.tick(far, now);
  CHECK(moving.status == TaskStatus::kRunning);
  CHECK(moving.phase == "move");
  CHECK(moving.nav_goal.has_value());

  WorldState in_zone = make_world(0.0, 0.0, 300);
  TaskOutput healing = task.tick(in_zone, now + Duration{1000});
  CHECK(healing.status == TaskStatus::kRunning);
  CHECK(healing.phase == "wait");

  in_zone.upstream.self_hp = 400;
  TaskOutput waiting_ammo = task.tick(in_zone, now + Duration{2000});
  CHECK(waiting_ammo.status == TaskStatus::kRunning);

  in_zone.supply_claimed_minutes = 1;
  TaskOutput done = task.tick(in_zone, now + Duration{3000});
  CHECK(done.status == TaskStatus::kSuccess);
}

void test_heal_ammo_timeout() {
  const PolicyConfig config = make_config();
  HealTask task(config, 400, Duration{420000});

  const TimePoint now{};
  task.start(heal_call("own_base_supply"), now);
  WorldState world = make_world(0.0, 0.0);
  CHECK(task.tick(world, now).status == TaskStatus::kRunning);

  TaskOutput out = task.tick(world, now + Duration{5000});
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "基地补给区免费发弹 5 s 内未到账");
}

void test_heal_outpost_requires_full_control() {
  const PolicyConfig config = make_config();
  HealTask task(config, 400, Duration{420000});

  const TimePoint now{};
  task.start(heal_call("own_outpost_buff"), now);
  WorldState world = make_world(5.0, 0.0);
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "己方前哨站未完全控制");
}

void test_heal_without_claim_succeeds_at_full_hp() {
  const PolicyConfig config = make_config();
  HealTask task(config, 400, Duration{420000});

  const TimePoint now{};
  task.start(heal_call("own_base_supply", false), now);
  WorldState world = make_world(0.0, 0.0);
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kSuccess);
}

void test_resource_sends_once_and_waits() {
  ResourceTask task;
  const TimePoint now{};
  task.start(TaskCall{"Resource", ResourceParams{"own_base_supply", 50}, Duration{0}}, now);

  WorldState world = make_world(0.0, 0.0);
  world.upstream.event.supply_zone_occupied = true;
  world.upstream.coins = 100;
  TaskOutput sent = task.tick(world, now);
  CHECK(sent.status == TaskStatus::kRunning);
  CHECK(sent.action.has_value());
  CHECK(sent.action->target == 50);
  CHECK(sent.action->verify == ActionVerify::kAmmoExchangedLocal);

  world.upstream.info1.ammo_exchanged_local = 50;
  TaskOutput done = task.tick(world, now + Duration{1000});
  CHECK(done.status == TaskStatus::kSuccess);
}

void test_resource_timeout() {
  ResourceTask task;
  const TimePoint now{};
  task.start(TaskCall{"Resource", ResourceParams{"own_base_supply", 50}, Duration{0}}, now);

  WorldState world = make_world(0.0, 0.0);
  world.upstream.event.supply_zone_occupied = true;
  world.upstream.coins = 100;
  CHECK(task.tick(world, now).status == TaskStatus::kRunning);

  TaskOutput out = task.tick(world, now + Duration{5000});
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "兑换 5 s 内未到账");
}

void test_resource_rejects_bad_amount_and_coins() {
  ResourceTask task;
  const TimePoint now{};
  task.start(TaskCall{"Resource", ResourceParams{"own_base_supply", 55}, Duration{0}}, now);
  TaskOutput bad = task.tick(make_world(0.0, 0.0), now);
  CHECK(bad.status == TaskStatus::kFailure);

  ResourceTask poor;
  poor.start(TaskCall{"Resource", ResourceParams{"own_base_supply", 50}, Duration{0}}, now);
  WorldState world = make_world(0.0, 0.0);
  world.upstream.event.supply_zone_occupied = true;
  world.upstream.coins = 10;
  TaskOutput out = poor.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "金币不足");
}

}  // namespace

int main() {
  test_heal_navigates_and_waits_for_ammo();
  test_heal_ammo_timeout();
  test_heal_outpost_requires_full_control();
  test_heal_without_claim_succeeds_at_full_hp();
  test_resource_sends_once_and_waits();
  test_resource_timeout();
  test_resource_rejects_bad_amount_and_coins();

  if (g_failures == 0) {
    std::printf("all heal_resource tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
