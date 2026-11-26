#pragma once

#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Respawn：待复活期间发送免费复活确认，复活成功即结束，不包含 Heal / 导航。
class RespawnTask : public Task {
 public:
  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return false;
  }
  void abort() override {}
  TaskState task_state() const override {
    return TaskState::kRespawn;
  }

 private:
  bool started_ = false;
};

}  // namespace sentry_decision
