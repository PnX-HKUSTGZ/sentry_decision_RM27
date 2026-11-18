#include "sentry_decision_core/tasks/scan_task.hpp"

#include <variant>

namespace sentry_decision {

bool ScanTask::can_start(const WorldState& world) const {
  return world.self.valid && world.upstream.self_hp > 0;
}

void ScanTask::start(const TaskCall& call, TimePoint) {
  started_ = std::holds_alternative<ScanParams>(call.params);
}

TaskOutput ScanTask::tick(const WorldState& world, TimePoint) {
  TaskOutput out;
  out.task_state = TaskState::kScan;
  out.phase = "scan";
  if (!started_) {
    out.status = TaskStatus::kFailure;
    out.detail = "Scan 参数类型不匹配";
    return out;
  }
  if (world.self.valid && world.upstream.self_hp <= 0) {
    out.status = TaskStatus::kFailure;
    out.detail = "机器人阵亡";
    return out;
  }
  out.status = TaskStatus::kRunning;
  return out;
}

void ScanTask::abort() {
  started_ = false;
}

}  // namespace sentry_decision
