#include "sentry_decision_core/nav_goal_tracker.hpp"

namespace sentry_decision {

bool NavGoalTracker::same(const Point2D& a, const Point2D& b) {
  return a.x == b.x && a.y == b.y && a.yaw == b.yaw;
}

NavGoalTracker::Step NavGoalTracker::update(const std::optional<Point2D>& goal) {
  if (goal.has_value()) {
    if (!last_goal_.has_value() || !same(*last_goal_, *goal)) {
      last_goal_ = goal;
      return Step{Decision::kSend, goal};
    }
    return Step{};
  }
  if (last_goal_.has_value()) {
    last_goal_.reset();
    return Step{Decision::kCancel, std::nullopt};
  }
  return Step{};
}

void NavGoalTracker::reset() {
  last_goal_.reset();
}

}  // namespace sentry_decision
