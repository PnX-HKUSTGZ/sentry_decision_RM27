#pragma once

#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Scan：原地扫描，不自动结束；动作层通过新 TaskPlan 切走。
// 旋转 / 检测 / 开火均由执行层与自瞄控制，本任务只下发 TaskState。
class ScanTask : public Task {
 public:
  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return true;
  }
  void abort() override;
  TaskState task_state() const override {
    return TaskState::kScan;
  }

 private:
  bool started_ = false;
};

}  // namespace sentry_decision
