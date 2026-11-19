#pragma once

#include <optional>
#include <string>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Navigate：导航到命名点，到达即成功。任务内不处理占点 / 扫描；
// 需要占点时由动作层下一轮选择 Scan / Outpost。
class NavigateTask : public Task {
 public:
  explicit NavigateTask(const PolicyConfig& config);

  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return true;
  }
  void abort() override;
  TaskState task_state() const override {
    return TaskState::kNavigate;
  }

 private:
  const PolicyConfig& config_;
  NavigateParams params_{};
  const Point2D* target_ = nullptr;
  bool started_ = false;
  bool invalid_ = false;
  TimePoint started_at_{};
  std::string invalid_reason_;
};

}  // namespace sentry_decision
