#include <cstdio>

#include "sentry_decision_core/nav_goal_tracker.hpp"

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

void test_edge_detection() {
  NavGoalTracker tracker;
  const Point2D home{-5.0, 3.0, 0.0};
  const Point2D attack{1.1, 1.1, 0.0};

  auto step = tracker.update(attack);
  CHECK(step.decision == NavGoalTracker::Decision::kSend);
  CHECK(step.goal.has_value());

  // 目标不变：不重发。
  step = tracker.update(attack);
  CHECK(step.decision == NavGoalTracker::Decision::kNone);

  // 目标变化：立即改派。
  step = tracker.update(home);
  CHECK(step.decision == NavGoalTracker::Decision::kSend);
  CHECK(step.goal->x == -5.0);

  // 撤销：取消。
  step = tracker.update(std::nullopt);
  CHECK(step.decision == NavGoalTracker::Decision::kCancel);
  CHECK(!tracker.current().has_value());

  // 再次撤销：无动作。
  step = tracker.update(std::nullopt);
  CHECK(step.decision == NavGoalTracker::Decision::kNone);

  // 取消后重新给目标：发送。
  step = tracker.update(home);
  CHECK(step.decision == NavGoalTracker::Decision::kSend);
}

}  // namespace

int main() {
  test_edge_detection();
  if (g_failures == 0) {
    std::printf("all nav goal tracker tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
