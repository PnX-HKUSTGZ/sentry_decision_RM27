#include "sentry_decision_core/tasks/respawn_task.hpp"

#include <variant>

#include "sentry_decision_core/action_factory.hpp"

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
  if (!world.upstream.valid) {
    out.status = TaskStatus::kFailure;
    out.detail = "裁判数据无效";
    return out;
  }
  out.metrics["alive"] = world.upstream.self_hp > 0 ? 1.0 : 0.0;
  out.metrics["can_free_resurrect"] = world.upstream.info1.can_free_resurrect ? 1.0 : 0.0;
  if (world.upstream.self_hp > 0) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "已复活";
    return out;
  }
  if (world.upstream.info1.can_free_resurrect) {
    out.action = make_free_resurrect();
  }
  out.status = TaskStatus::kRunning;
  return out;
}

}  // namespace sentry_decision
