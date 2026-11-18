#include "sentry_decision_core/outpost_task.hpp"

#include <variant>

namespace sentry_decision {

bool OutpostTask::can_start(const WorldState& world) const {
  return world.self.valid;
}

void OutpostTask::start(const TaskCall& call, TimePoint) {
  const auto* params = std::get_if<OutpostParams>(&call.params);
  started_ = params != nullptr;
  if (params != nullptr) {
    params_ = *params;
  }
}

TaskOutput OutpostTask::tick(const WorldState& world, TimePoint) {
  TaskOutput out;
  out.task_state = TaskState::kOutpost;
  out.phase = "attack";
  if (!started_) {
    out.status = TaskStatus::kFailure;
    out.detail = "Outpost 参数类型不匹配";
    return out;
  }

  const int hp = params_.target_side == Side::kOwn ? world.upstream.our_outpost_hp
                                                   : world.upstream.enemy_outpost_hp;
  const OutpostControl control = params_.target_side == Side::kOwn
                                     ? world.upstream.our_outpost_control
                                     : world.upstream.enemy_outpost_control;
  out.metrics["outpost_hp"] = static_cast<double>(hp);
  out.metrics["outpost_control"] = static_cast<double>(control);

  if (hp <= 0) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "目标前哨站已被击毁";
    return out;
  }
  if (control != OutpostControl::kNotOwned) {
    out.status = TaskStatus::kFailure;
    out.detail = "目标前哨站已被己方控制";
    return out;
  }

  out.status = TaskStatus::kRunning;
  return out;
}

void OutpostTask::abort() {
  started_ = false;
}

}  // namespace sentry_decision
