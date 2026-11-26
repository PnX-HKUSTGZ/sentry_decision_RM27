#pragma once

#include <memory>
#include <optional>
#include <string>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/task.hpp"
#include "sentry_decision_core/tasks/resource_task.hpp"

namespace sentry_decision {

// 独立的 Resource 辅助栏：只负责“在兑换点补弹”，不占用主 TaskRuntime，
// 不产生 DecisionEpoch，也不写 TaskState。主栏的 nav_goal / task_state 不受影响。
class ResourceController {
 public:
  explicit ResourceController(const PolicyConfig& config);

  void tick(const WorldState& world, TimePoint now);

  std::optional<ActionRequest> action() const {
    return action_;
  }
  std::optional<TaskSnapshot> snapshot() const {
    return snapshot_;
  }
  std::optional<TaskResult> take_result();

  void reset();

 private:
  struct ExchangePoint {
    std::string name;
    double radius = 0.0;
  };

  std::optional<ExchangePoint> find_exchange_point(const WorldState& world) const;
  bool free_supply_ammo_pending(const WorldState& world) const;
  int choose_amount(const WorldState& world, int max_amount) const;
  void start_resource(const ExchangePoint& point, int amount, const WorldState& world,
                      TimePoint now);
  void tick_resource(const WorldState& world, TimePoint now);
  void clear_active();

  const PolicyConfig& config_;
  bool enabled_ = true;
  int low_ammo_ = 80;
  int batch_ = 100;
  int reserve_coins_ = 150;
  Duration dwell_{500};
  Duration retry_{2000};

  std::unique_ptr<ResourceTask> task_;
  std::optional<TaskCall> call_;
  std::optional<TaskSnapshot> snapshot_;
  std::optional<ActionRequest> action_;
  std::optional<TaskResult> result_;
  TimePoint started_at_{};
  TimePoint retry_after_{};
  TimePoint zone_since_{};
  std::string zone_name_;
  bool in_zone_ = false;
};

}  // namespace sentry_decision
