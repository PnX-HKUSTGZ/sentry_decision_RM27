#include <cstdio>

#include "sentry_decision_core/tasks/outpost_task.hpp"
#include "sentry_decision_core/tasks/respawn_task.hpp"

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

TaskCall outpost_call(Side side) {
  return TaskCall{"Outpost", OutpostParams{side}, Duration{0}};
}

void test_outpost_running_and_success() {
  OutpostTask task;
  const TimePoint now{};
  task.start(outpost_call(Side::kEnemy), now);

  WorldState world;
  world.self.valid = true;
  world.upstream.valid = true;
  world.upstream.enemy_outpost_hp = 1000;
  world.upstream.enemy_outpost_control = OutpostControl::kNotOwned;
  TaskOutput running = task.tick(world, now);
  CHECK(running.status == TaskStatus::kRunning);
  CHECK(running.task_state == TaskState::kOutpost);
  CHECK(running.metrics.at("outpost_hp") == 1000.0);

  world.upstream.enemy_outpost_hp = 0;
  TaskOutput done = task.tick(world, now);
  CHECK(done.status == TaskStatus::kSuccess);
}

void test_outpost_fails_if_owned() {
  OutpostTask task;
  const TimePoint now{};
  task.start(outpost_call(Side::kOwn), now);

  WorldState world;
  world.self.valid = true;
  world.upstream.valid = true;
  world.upstream.our_outpost_hp = 1000;
  world.upstream.our_outpost_control = OutpostControl::kOwnTemporary;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "目标前哨站已被己方控制");
}

void test_outpost_own_side_uses_own_hp() {
  OutpostTask task;
  const TimePoint now{};
  task.start(outpost_call(Side::kOwn), now);

  WorldState world;
  world.self.valid = true;
  world.upstream.valid = true;
  world.upstream.our_outpost_hp = 500;
  world.upstream.enemy_outpost_hp = 0;
  world.upstream.our_outpost_control = OutpostControl::kNotOwned;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kRunning);
  CHECK(out.metrics.at("outpost_hp") == 500.0);
}

void test_outpost_rejects_wrong_params() {
  OutpostTask task;
  const TimePoint now{};
  task.start(TaskCall{"Outpost", ScanParams{}, Duration{0}}, now);
  WorldState world;
  world.self.valid = true;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
}

void test_respawn_waits_then_succeeds() {
  RespawnTask task;
  const TimePoint now{};
  task.start(TaskCall{"Respawn", RespawnParams{}, Duration{0}}, now);
  CHECK(!task.can_abort());

  WorldState world;
  world.upstream.valid = true;
  world.upstream.self_hp = 0;
  TaskOutput waiting = task.tick(world, now);
  CHECK(waiting.status == TaskStatus::kRunning);
  CHECK(waiting.task_state == TaskState::kRespawn);
  CHECK(!waiting.action.has_value());

  world.upstream.info1.can_free_resurrect = true;
  TaskOutput confirming = task.tick(world, now);
  CHECK(confirming.status == TaskStatus::kRunning);
  CHECK(confirming.action.has_value());
  CHECK(confirming.action->kind == DecisionActionKind::kFreeResurrect);
  CHECK(confirming.action->target == 1);

  world.upstream.self_hp = 400;
  TaskOutput done = task.tick(world, now);
  CHECK(done.status == TaskStatus::kSuccess);
}

void test_respawn_fails_without_referee_data() {
  RespawnTask task;
  const TimePoint now{};
  task.start(TaskCall{"Respawn", RespawnParams{}, Duration{0}}, now);
  TaskOutput out = task.tick(WorldState{}, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "裁判数据无效");
}

void test_respawn_rejects_wrong_params() {
  RespawnTask task;
  const TimePoint now{};
  task.start(TaskCall{"Respawn", ScanParams{}, Duration{0}}, now);
  TaskOutput out = task.tick(WorldState{}, now);
  CHECK(out.status == TaskStatus::kFailure);
}

}  // namespace

int main() {
  test_outpost_running_and_success();
  test_outpost_fails_if_owned();
  test_outpost_own_side_uses_own_hp();
  test_outpost_rejects_wrong_params();
  test_respawn_waits_then_succeeds();
  test_respawn_rejects_wrong_params();
  test_respawn_fails_without_referee_data();

  if (g_failures == 0) {
    std::printf("all outpost_respawn tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
