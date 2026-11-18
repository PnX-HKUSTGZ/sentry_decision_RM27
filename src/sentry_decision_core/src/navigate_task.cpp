#include "sentry_decision_core/navigate_task.hpp"

#include <cmath>
#include <utility>
#include <variant>

namespace sentry_decision {

NavigateTask::NavigateTask(const PolicyConfig& config) : config_(config) {}

bool NavigateTask::can_start(const WorldState& world) const {
  return world.self.valid && world.nav.valid;
}

void NavigateTask::start(const TaskCall& call, TimePoint) {
  started_ = true;
  invalid_ = false;
  invalid_reason_.clear();
  target_ = nullptr;

  const auto* params = std::get_if<NavigateParams>(&call.params);
  if (params == nullptr) {
    invalid_ = true;
    invalid_reason_ = "Navigate 参数类型不匹配";
    return;
  }
  params_ = *params;
  target_ = config_.find_point(params_.target);
  if (target_ == nullptr) {
    invalid_ = true;
    invalid_reason_ = "命名点不存在: " + params_.target;
  }
}

TaskOutput NavigateTask::tick(const WorldState& world, TimePoint) {
  TaskOutput out;
  out.task_state = TaskState::kNavigate;
  out.phase = "move";

  if (!started_ || invalid_ || target_ == nullptr) {
    out.status = TaskStatus::kFailure;
    out.detail = invalid_reason_.empty() ? "Navigate 未启动" : invalid_reason_;
    return out;
  }

  if (!world.nav.valid) {
    out.status = TaskStatus::kFailure;
    out.detail = "导航状态无效";
    return out;
  }

  if (world.nav.failed) {
    out.status = TaskStatus::kFailure;
    out.detail = "导航失败";
    return out;
  }

  double distance = 0.0;
  if (world.self.valid) {
    distance = std::hypot(world.self.pose.x - target_->x, world.self.pose.y - target_->y);
    out.metrics["distance_remaining"] = distance;
  }

  if (world.nav.reached) {
    if (world.self.valid && distance > params_.arrive_radius) {
      out.status = TaskStatus::kFailure;
      out.detail = "导航已结束但不在到达半径内";
      return out;
    }
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "到达 " + params_.target;
    return out;
  }

  out.status = TaskStatus::kRunning;
  out.nav_goal = *target_;
  return out;
}

void NavigateTask::abort() {
  started_ = false;
  target_ = nullptr;
}

}  // namespace sentry_decision
