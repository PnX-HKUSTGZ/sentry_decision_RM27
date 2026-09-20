#include <cmath>
#include <cstdio>

#include "sentry_decision_sim/nav_simulator.hpp"

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

void test_invalid_before_goal() {
  NavSimulator nav;
  SelfState self;
  CHECK(nav.odometry(&self));
  CHECK(!nav.status().valid);
}

void test_moves_and_reaches_goal() {
  const TimePoint t0{};
  NavSimulator nav(2.0, 0.1);
  nav.set_pose(Point2D{0.0, 0.0, 0.0});
  nav.send_goal(Point2D{2.0, 0.0, 1.25});

  nav.update(t0);
  SelfState self;
  CHECK(nav.odometry(&self));
  CHECK(self.pose.x == 0.0);

  nav.update(t0 + Duration{500});
  CHECK(nav.odometry(&self));
  CHECK(std::abs(self.pose.x - 1.0) < 1e-9);
  CHECK(!nav.status().reached);

  nav.update(t0 + Duration{1000});
  CHECK(nav.odometry(&self));
  CHECK(self.pose.x == 2.0);
  CHECK(self.pose.y == 0.0);
  CHECK(self.pose.yaw == 1.25);
  CHECK(nav.status().reached);
  CHECK(!nav.status().failed);
}

void test_cancel_and_fail() {
  const TimePoint t0{};
  NavSimulator nav;
  nav.send_goal(Point2D{5.0, 0.0, 0.0});
  nav.cancel_goal();
  nav.update(t0);
  CHECK(!nav.status().current_goal.has_value());
  CHECK(!nav.status().reached);

  nav.send_goal(Point2D{5.0, 0.0, 0.0});
  nav.fail_current_goal();
  CHECK(nav.status().failed);  // 立即生效，无需等待 update
  CHECK(!nav.status().reached);
  nav.update(t0 + Duration{100});
  CHECK(nav.status().failed);
  CHECK(!nav.status().reached);
}

}  // namespace

int main() {
  test_invalid_before_goal();
  test_moves_and_reaches_goal();
  test_cancel_and_fail();
  if (g_failures == 0) {
    std::printf("all sim nav_simulator tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
