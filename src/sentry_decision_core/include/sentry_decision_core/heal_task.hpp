#pragma once

#include <string>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// Heal：导航到基地补给区 / 己方完全控制的前哨站增益点，等待回满血；
// 基地补给区同时确认累积免费发弹量到账。
class HealTask : public Task {
 public:
  HealTask(const PolicyConfig& config, int max_hp, Duration match_duration);

  bool can_start(const WorldState& world) const override;
  void start(const TaskCall& call, TimePoint now) override;
  TaskOutput tick(const WorldState& world, TimePoint now) override;
  bool can_abort() const override {
    return true;
  }
  void abort() override;
  TaskState task_state() const override {
    return TaskState::kHeal;
  }

 private:
  const PolicyConfig& config_;
  int max_hp_ = 0;
  Duration match_duration_{0};
  HealParams params_{};
  const Point2D* target_ = nullptr;
  double zone_radius_ = 0.0;
  bool started_ = false;
  bool invalid_ = false;
  std::string invalid_reason_;
  bool in_zone_ = false;
  TimePoint in_zone_since_{};
  bool ammo_wait_started_ = false;
  TimePoint ammo_wait_since_{};
};

}  // namespace sentry_decision
