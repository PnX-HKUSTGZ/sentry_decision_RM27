#pragma once

#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Outpost：只下发 TaskState=kOutpost，由自瞄攻击目标前哨站。
// 不导航；进入时目标必须是未被己方控制（通常为敌方控制）且血量 > 0。
class OutpostTask : public Task {
 public:
  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return true;
  }
  void abort() override;
  TaskState task_state() const override {
    return TaskState::kOutpost;
  }

 private:
  OutpostParams params_{};
  bool started_ = false;
};

}  // namespace sentry_decision
