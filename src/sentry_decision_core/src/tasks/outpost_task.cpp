#include "sentry_decision_core/tasks/outpost_task.hpp"

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

  if (!world.upstream.valid) {
    out.status = TaskStatus::kFailure;
    out.detail = "裁判数据无效";
    return out;
  }

  const bool own_side = params_.target_side == Side::kOwn;
  const int hp = own_side ? world.upstream.our_outpost_hp : world.upstream.enemy_outpost_hp;
  const OutpostControl control = own_side ? world.upstream.our_outpost_control
                                          : world.upstream.enemy_outpost_control;
  const OutpostOwner owner = own_side ? world.upstream.our_outpost_owner
                                      : world.upstream.enemy_outpost_owner;
  out.metrics["outpost_hp"] = static_cast<double>(hp);
  out.metrics["outpost_control"] = static_cast<double>(control);
  out.metrics["outpost_owner"] = static_cast<double>(owner);

  // 只允许攻击敌方归属的前哨站；己方归属 / 未知一律不攻击。
  if (owner != OutpostOwner::kEnemy) {
    out.status = TaskStatus::kFailure;
    out.detail = owner == OutpostOwner::kOwn ? "目标前哨站归属我方" : "目标前哨站归属未知";
    return out;
  }
  if (hp <= 0) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "目标前哨站已被击毁";
    return out;
  }
  if (control == OutpostControl::kOwnFull) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "目标前哨站已由我方完全控制";
    return out;
  }

  out.status = TaskStatus::kRunning;
  return out;
}

void OutpostTask::abort() {
  started_ = false;
}

}  // namespace sentry_decision
