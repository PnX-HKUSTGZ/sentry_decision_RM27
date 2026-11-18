#include "sentry_decision_core/task_runtime.hpp"

#include <utility>

namespace sentry_decision {

void TaskRegistry::register_task(const TaskId& id, Factory factory) {
  factories_[id] = std::move(factory);
}

std::unique_ptr<Task> TaskRegistry::create(const TaskId& id) const {
  const auto it = factories_.find(id);
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second();
}

TaskRuntime::TaskRuntime(TaskRegistry& registry) : registry_(registry) {}

void TaskRuntime::push_epoch(ReplanReason reason, TimePoint now,
                             const std::optional<TaskId>& task) {
  epochs_.push_back(DecisionEpoch{reason, now, task});
}

void TaskRuntime::start(const TaskCall& call, const WorldState& world, TimePoint now) {
  std::unique_ptr<Task> task = registry_.create(call.task);
  if (!task) {
    TaskResult result;
    result.task = call.task;
    result.status = TaskStatus::kFailure;
    result.finished_at = now;
    result.reason = "unknown task";
    last_result_ = std::move(result);
    push_epoch(ReplanReason::kTaskFailed, now, call.task);
    return;
  }

  if (!task->can_start(world)) {
    TaskResult result;
    result.task = call.task;
    result.status = TaskStatus::kFailure;
    result.finished_at = now;
    result.reason = "task cannot start";
    last_result_ = std::move(result);
    push_epoch(ReplanReason::kTaskFailed, now, call.task);
    return;
  }

  auto active = std::make_unique<ActiveTask>();
  active->task = std::move(task);
  active->call = call;
  active->started_at = now;
  active->snapshot.task = call.task;
  active->snapshot.call = call;
  active->snapshot.status = TaskStatus::kRunning;
  active->snapshot.started_at = now;
  active->task->start(call, now);
  active->snapshot.task_state = active->task->task_state();
  active->snapshot.can_abort = active->task->can_abort();
  active->output.task_state = active->snapshot.task_state;
  active_ = std::move(active);
  pending_plan_.reset();
}

void TaskRuntime::finish(TaskStatus status, TimePoint now, const std::string& reason,
                         const std::map<std::string, double>& metrics) {
  if (!active_) {
    return;
  }

  TaskResult result;
  result.task = active_->call.task;
  result.status = status;
  result.finished_at = now;
  result.elapsed = std::chrono::duration_cast<Duration>(now - active_->started_at);
  result.reason = reason;
  result.metrics = metrics;
  last_result_ = std::move(result);

  ReplanReason replan = ReplanReason::kTaskFinished;
  switch (status) {
    case TaskStatus::kFailure:
      replan = ReplanReason::kTaskFailed;
      break;
    case TaskStatus::kTimeout:
      replan = ReplanReason::kTaskTimeout;
      break;
    case TaskStatus::kAborted:
      replan = ReplanReason::kTaskAborted;
      break;
    case TaskStatus::kSuccess:
    case TaskStatus::kIdle:
    case TaskStatus::kRunning:
    default:
      break;
  }
  const TaskId task = active_->call.task;
  active_.reset();
  push_epoch(replan, now, task);
}

void TaskRuntime::apply(const TaskPlan& plan, const WorldState& world, TimePoint now) {
  if (plan.valid_for.count() > 0 && now - plan.created_at > plan.valid_for) {
    push_epoch(ReplanReason::kPlanExpired, now, std::nullopt);
    return;
  }

  if (!plan.call.has_value()) {
    if (!active_) {
      return;
    }
    if (!active_->task->can_abort()) {
      pending_plan_ = plan;
      active_->snapshot.abort_block_reason = "pending empty plan";
      return;
    }
    active_->task->abort();
    finish(TaskStatus::kAborted, now, "plan cleared", {});
    return;
  }

  if (active_ && active_->call == *plan.call) {
    // 策略可能先请求切换、随后又决定继续当前任务；此时要撤销旧的 pending，
    // 否则当前任务结束后会意外启动已经过时的计划。
    pending_plan_.reset();
    active_->snapshot.abort_block_reason.clear();
    return;
  }

  if (active_ && !active_->task->can_abort()) {
    pending_plan_ = plan;
    active_->snapshot.abort_block_reason = "pending task switch";
    return;
  }

  if (active_) {
    active_->task->abort();
    finish(TaskStatus::kAborted, now, "preempted", {});
  }
  start(*plan.call, world, now);
}

void TaskRuntime::apply_pending(const WorldState& world, TimePoint now) {
  if (!pending_plan_.has_value()) {
    return;
  }
  if (active_ && !active_->task->can_abort()) {
    return;
  }
  const TaskPlan plan = *pending_plan_;
  pending_plan_.reset();
  apply(plan, world, now);
}

void TaskRuntime::tick(const WorldState& world, TimePoint now) {
  if (pending_plan_.has_value()) {
    apply_pending(world, now);
  }
  if (!active_) {
    return;
  }

  if (active_->call.timeout.count() > 0 && now - active_->started_at >= active_->call.timeout) {
    active_->task->abort();
    finish(TaskStatus::kTimeout, now, "task timeout", active_->snapshot.metrics);
    return;
  }

  TaskOutput output = active_->task->tick(world, now);
  active_->output = output;
  active_->snapshot.status = output.status;
  active_->snapshot.task_state = output.task_state;
  active_->snapshot.phase = std::move(output.phase);
  active_->snapshot.progress = output.progress;
  active_->snapshot.detail = output.detail;
  active_->snapshot.metrics = std::move(output.metrics);
  active_->snapshot.elapsed = std::chrono::duration_cast<Duration>(now - active_->started_at);
  active_->snapshot.can_abort = active_->task->can_abort();

  if (output.status != TaskStatus::kRunning) {
    finish(output.status, now, active_->snapshot.detail, active_->snapshot.metrics);
  }
}

std::optional<TaskSnapshot> TaskRuntime::snapshot() const {
  if (!active_) {
    return std::nullopt;
  }
  return active_->snapshot;
}

std::optional<Point2D> TaskRuntime::nav_goal() const {
  if (!active_ || !active_->output.nav_goal.has_value()) {
    return std::nullopt;
  }
  return active_->output.nav_goal;
}

std::optional<ActionRequest> TaskRuntime::action() const {
  if (!active_ || !active_->output.action.has_value()) {
    return std::nullopt;
  }
  return active_->output.action;
}

std::optional<TaskResult> TaskRuntime::take_result() {
  if (!last_result_.has_value()) {
    return std::nullopt;
  }
  TaskResult result = std::move(*last_result_);
  last_result_.reset();
  return result;
}

std::optional<DecisionEpoch> TaskRuntime::take_epoch() {
  if (epochs_.empty()) {
    return std::nullopt;
  }
  DecisionEpoch epoch = epochs_.front();
  epochs_.pop_front();
  return epoch;
}

TaskState TaskRuntime::task_state() const {
  return active_ ? active_->snapshot.task_state : TaskState::kIdle;
}

}  // namespace sentry_decision
