#pragma once

#include <string>

#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Resource：在己方基地补给区 / 己方完全控制且存活的前哨站增益点，向裁判
// 兑换允许发弹量。状态型任务，不导航；只发送一次，5 s 内等累计值到账。
// 由独立的 ResourceController 辅助栏执行，不占主任务栏。
class ResourceTask : public Task {
 public:
  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return true;
  }
  void abort() override;
  TaskState task_state() const override {
    return TaskState::kResource;
  }

 private:
  enum class ZoneKind {
    kBaseSupply,
    kOwnOutpost,
    kEnemyOutpost,
  };

  ResourceParams params_{};
  ZoneKind zone_ = ZoneKind::kBaseSupply;
  bool started_ = false;
  bool invalid_ = false;
  bool sent_ = false;
  std::string invalid_reason_;
  int baseline_ = 0;
  int target_ = 0;
  ActionRequest request_{};
  TimePoint sent_at_{};
};

}  // namespace sentry_decision
