#include <cstdio>

#include "sentry_decision_core/safety_supervisor.hpp"

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

WorldState make_world() {
  WorldState world;
  world.referee.valid = true;
  world.self.valid = true;
  return world;
}

DecisionOutput make_output() {
  DecisionOutput output;
  output.nav_goal = Point2D{1.0, 2.0, 0.0};
  output.cmd_vel = Twist{5.0, -5.0, 20.0};
  return output;
}

void test_clamp_when_valid() {
  const SafetySupervisor supervisor(SafetyLimits{1.0, 2.0, 3.0, true, true});
  const SafetyResult result = supervisor.apply(make_world(), make_output());
  CHECK(!result.emergency);
  CHECK(result.output.nav_goal.has_value());
  CHECK(result.output.cmd_vel.has_value());
  CHECK(result.output.cmd_vel->vx == 1.0);
  CHECK(result.output.cmd_vel->vy == -2.0);
  CHECK(result.output.cmd_vel->wz == 3.0);
}

void test_emergency_on_stale_referee() {
  WorldState world = make_world();
  world.referee.valid = false;
  const SafetySupervisor supervisor;
  const SafetyResult result = supervisor.apply(world, make_output());
  CHECK(result.emergency);
  CHECK(!result.output.nav_goal.has_value());
  CHECK(result.output.cmd_vel.has_value());
  CHECK(result.output.cmd_vel->vx == 0.0);
  CHECK(result.output.cmd_vel->wz == 0.0);
}

void test_emergency_on_stale_odometry() {
  WorldState world = make_world();
  world.self.valid = false;
  const SafetySupervisor supervisor;
  const SafetyResult result = supervisor.apply(world, make_output());
  CHECK(result.emergency);
  CHECK(!result.output.nav_goal.has_value());
}

void test_valid_without_cmd_vel() {
  DecisionOutput output;
  output.nav_goal = Point2D{1.0, 2.0, 0.0};
  const SafetySupervisor supervisor;
  const SafetyResult result = supervisor.apply(make_world(), output);
  CHECK(!result.emergency);
  CHECK(result.output.nav_goal.has_value());
  CHECK(!result.output.cmd_vel.has_value());
}

}  // namespace

int main() {
  test_clamp_when_valid();
  test_emergency_on_stale_referee();
  test_emergency_on_stale_odometry();
  test_valid_without_cmd_vel();
  if (g_failures == 0) {
    std::printf("all safety supervisor tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
