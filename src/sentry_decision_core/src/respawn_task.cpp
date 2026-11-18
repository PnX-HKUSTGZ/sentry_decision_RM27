#include "sentry_decision_core/respawn_task.hpp"

#include <variant>

namespace sentry_decision {

bool RespawnTask::can_start(const WorldState&) const {
  return true;
}

void RespawnTask::start(const TaskCall& call, TimePoint) {
  started_ = std::holds_alternative<RespawnParams>(call.params);
}

TaskOutput RespawnTask::tick(const WorldState& world, TimePoint) {
  TaskOutput out;
  out.task_state = TaskState::kRespawn;
  out.phase = "wait_revive";
  if (!started_) {
    out.status = TaskStatus::kFailure;
    out.detail = "Respawn 参数类型不匹配";
    return out;
  }
  if (world.upstream.self_hp > 0) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "已复活";
    return out;
  }
  out.status = TaskStatus::kRunning;
  return out;
}

}  // namespace sentry_decision
