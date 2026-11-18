#include <cstdio>
#include <string>

#include "sentry_decision_core/tasks/navigate_task.hpp"
#include "sentry_decision_core/tasks/scan_task.hpp"

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
  config.points["own_base_supply"] = Point2D{1.0, 2.0, 0.0};
  return config;
}

WorldState make_world(double x, double y) {
  WorldState world;
  world.self.valid = true;
  world.self.pose = Point2D{x, y, 0.0};
  world.nav.valid = true;
  world.upstream.self_hp = 400;
  return world;
}

TaskCall navigate_call(const std::string& target) {
  return TaskCall{"Navigate", NavigateParams{target, 0.2}, Duration{0}};
}

void test_navigate_running_then_success() {
  const PolicyConfig config = make_config();
  NavigateTask task(config);
  const TimePoint now{};
  task.start(navigate_call("own_base_supply"), now);

  WorldState world = make_world(0.0, 0.0);
  TaskOutput running = task.tick(world, now);
  CHECK(running.status == TaskStatus::kRunning);
  CHECK(running.task_state == TaskState::kNavigate);
  CHECK(running.nav_goal.has_value());
  CHECK(running.nav_goal->x == 1.0);
  CHECK(running.nav_goal->y == 2.0);

  world.nav.reached = true;
  world.self.pose = Point2D{1.1, 2.0, 0.0};
  TaskOutput done = task.tick(world, now);
  CHECK(done.status == TaskStatus::kSuccess);
  CHECK(done.progress == 1.0f);
}

void test_navigate_failure_nav_failed() {
  const PolicyConfig config = make_config();
  NavigateTask task(config);
  const TimePoint now{};
  task.start(navigate_call("own_base_supply"), now);

  WorldState world = make_world(0.0, 0.0);
  world.nav.failed = true;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "导航失败");
}

void test_navigate_failure_missing_point() {
  const PolicyConfig config = make_config();
  NavigateTask task(config);
  const TimePoint now{};
  task.start(navigate_call("missing"), now);

  TaskOutput out = task.tick(make_world(0.0, 0.0), now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail.find("命名点不存在") != std::string::npos);
}

void test_navigate_failure_outside_radius() {
  const PolicyConfig config = make_config();
  NavigateTask task(config);
  const TimePoint now{};
  task.start(navigate_call("own_base_supply"), now);

  WorldState world = make_world(0.0, 0.0);
  world.nav.reached = true;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
  CHECK(out.detail == "导航已结束但不在到达半径内");
}

void test_scan_running() {
  ScanTask task;
  const TimePoint now{};
  task.start(TaskCall{"Scan", ScanParams{}, Duration{0}}, now);

  TaskOutput out = task.tick(make_world(0.0, 0.0), now);
  CHECK(out.status == TaskStatus::kRunning);
  CHECK(out.task_state == TaskState::kScan);
  CHECK(out.phase == "scan");
}

void test_scan_rejects_wrong_params() {
  ScanTask task;
  const TimePoint now{};
  task.start(TaskCall{"Scan", HealParams{}, Duration{0}}, now);

  TaskOutput out = task.tick(make_world(0.0, 0.0), now);
  CHECK(out.status == TaskStatus::kFailure);
}

void test_scan_fails_when_dead() {
  ScanTask task;
  const TimePoint now{};
  task.start(TaskCall{"Scan", ScanParams{}, Duration{0}}, now);

  WorldState world = make_world(0.0, 0.0);
  world.upstream.self_hp = 0;
  TaskOutput out = task.tick(world, now);
  CHECK(out.status == TaskStatus::kFailure);
}

}  // namespace

int main() {
  test_navigate_running_then_success();
  test_navigate_failure_nav_failed();
  test_navigate_failure_missing_point();
  test_navigate_failure_outside_radius();
  test_scan_running();
  test_scan_rejects_wrong_params();
  test_scan_fails_when_dead();

  if (g_failures == 0) {
    std::printf("all navigate_scan tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
