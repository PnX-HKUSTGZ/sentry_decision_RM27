#include <cmath>
#include <cstdio>

#include "sentry_decision_io/pose_transform.hpp"

using sentry_decision::SelfState;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

bool near(double a, double b, double tol = 1e-9) {
  return std::fabs(a - b) <= tol;
}

SelfState make_odom(double x, double y, double yaw, double vx, double vy, double wz) {
  SelfState s;
  s.pose.x = x;
  s.pose.y = y;
  s.pose.yaw = yaw;
  s.vx = vx;
  s.vy = vy;
  s.wz = wz;
  s.valid = true;
  return s;
}

void test_identity() {
  const auto out =
      sentry_decision_io::pose_to_map({0.0, 0.0, 0.0}, make_odom(1.0, -2.0, 0.5, 0.3, -0.4, 1.2));
  CHECK(near(out.pose.x, 1.0));
  CHECK(near(out.pose.y, -2.0));
  CHECK(near(out.pose.yaw, 0.5));
  CHECK(near(out.vx, 0.3));
  CHECK(near(out.vy, -0.4));
  CHECK(near(out.wz, 1.2));
  CHECK(out.valid);
}

void test_translation_only() {
  const auto out =
      sentry_decision_io::pose_to_map({-11.7, 2.9, 0.0}, make_odom(0.5, 0.25, 0.0, 1.0, 0.0, 0.0));
  CHECK(near(out.pose.x, -11.2));
  CHECK(near(out.pose.y, 3.15));
}

void test_translation_and_rotation() {
  const double half_pi = 1.5707963267948966;
  const auto out =
      sentry_decision_io::pose_to_map({1.0, 2.0, half_pi}, make_odom(1.0, 0.0, 0.0, 1.0, 0.0, 0.0));
  CHECK(near(out.pose.x, 1.0));
  CHECK(near(out.pose.y, 3.0));
  CHECK(near(out.pose.yaw, half_pi));
  CHECK(near(out.vx, 0.0));
  CHECK(near(out.vy, 1.0));
}

}  // namespace

int main() {
  test_identity();
  test_translation_only();
  test_translation_and_rotation();

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_pose_transform passed\n");
  return 0;
}
