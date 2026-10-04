#include "sentry_decision_sim/nav_simulator.hpp"

#include <chrono>
#include <cmath>

namespace sentry_decision_sim {

NavSimulator::NavSimulator(double speed, double tolerance) : speed_(speed), tolerance_(tolerance) {
  self_.valid = true;
  nav_.valid = false;
}

void NavSimulator::set_pose(const sentry_decision::Point2D& pose) {
  self_.pose = pose;
}

void NavSimulator::fail_current_goal() {
  failed_ = true;
  reached_ = false;
  // 立即反映到状态，不等下一次 update()，避免调用方读到旧值。
  nav_.failed = true;
  nav_.reached = false;
}

bool NavSimulator::odometry(sentry_decision::SelfState* out) const {
  *out = self_;
  return self_.valid;
}

void NavSimulator::send_goal(const sentry_decision::Point2D& goal) {
  goal_ = goal;
  reached_ = false;
  failed_ = false;
  nav_.current_goal = goal;
  nav_.reached = false;
  nav_.failed = false;
  nav_.valid = true;
}

void NavSimulator::cancel_goal() {
  goal_.reset();
  reached_ = false;
  failed_ = false;
  nav_.current_goal.reset();
  nav_.reached = false;
  nav_.failed = false;
}

sentry_decision::NavState NavSimulator::status() const {
  return nav_;
}

void NavSimulator::update(sentry_decision::TimePoint now) {
  auto dt = sentry_decision::Duration{0};
  if (has_last_now_) {
    dt = std::chrono::duration_cast<sentry_decision::Duration>(now - last_now_);
  }
  last_now_ = now;
  has_last_now_ = true;

  if (!failed_ && goal_.has_value() && !reached_) {
    const double dx = goal_->x - self_.pose.x;
    const double dy = goal_->y - self_.pose.y;
    const double dist = std::hypot(dx, dy);
    const double step = speed_ * (static_cast<double>(dt.count()) / 1000.0);
    if (dist <= tolerance_ || step >= dist) {
      self_.pose.x = goal_->x;
      self_.pose.y = goal_->y;
      self_.pose.yaw = goal_->yaw;  // 到达时对齐目标朝向
      reached_ = true;
    } else if (step > 0.0 && dist > 0.0) {
      self_.pose.x += dx / dist * step;
      self_.pose.y += dy / dist * step;
      self_.pose.yaw = std::atan2(dy, dx);
    }
  }

  self_.stamp = now;
  self_.valid = true;
  nav_.pose = self_.pose;
  nav_.current_goal = goal_;
  nav_.reached = reached_;
  nav_.failed = failed_;
  nav_.valid = true;
  nav_.stamp = now;
}

}  // namespace sentry_decision_sim
