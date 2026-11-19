#pragma once

#include <deque>
#include <optional>

#include "sentry_decision_core/task_policy.hpp"
#include "sentry_decision_core/task_runtime.hpp"

namespace sentry_decision {

// Task 层组合根：Policy（纯函数）+ Runtime（有状态）+ 2027 默认 Task 表。
// 负责把 TaskOutput 暴露给上层 IO；不直接接触 ROS。
class TaskRunner {
 public:
  TaskRunner(const PolicyConfig& config, int max_hp, Duration match_duration);

  void tick(const WorldState& world, TimePoint now);

  std::optional<Point2D> nav_goal() const {
    return runtime_.nav_goal();
  }
  std::optional<ActionRequest> action() const {
    return runtime_.action();
  }
  TaskState task_state() const {
    return runtime_.task_state();
  }
  std::optional<TaskSnapshot> snapshot() const {
    return runtime_.snapshot();
  }
  const std::optional<TaskResult>& previous_task() const {
    return history_.previous_task;
  }
  std::optional<TaskResult> take_result();
  std::optional<DecisionEpoch> take_epoch();

 private:
  TaskRegistry registry_;
  TaskRuntime runtime_;
  RuleBasedTaskPolicy policy_;
  HistoryView history_;
  Duration match_duration_{0};
  bool belief_initialized_ = false;
  int last_self_ammo_ = 0;
  int last_local_exchange_ = 0;
  int supply_claimed_minutes_ = 0;
  std::optional<TaskResult> last_result_;
  TimePoint last_decision_at_{};
  bool decision_due_ = true;
  std::deque<DecisionEpoch> epochs_;
};

}  // namespace sentry_decision
