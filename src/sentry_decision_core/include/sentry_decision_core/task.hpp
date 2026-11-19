#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 2027 Task 层的纯逻辑契约。详见 docs/STRATEGY_LAYER.md §10 与
// docs/TASK_CATALOG.md。

using TaskId = std::string;

enum class TaskStatus : std::uint8_t {
  kIdle,
  kRunning,
  kSuccess,
  kFailure,
  kTimeout,
  kAborted,
};

enum class Side : std::uint8_t {
  kOwn,
  kEnemy,
};

struct NavigateParams {
  std::string target;
  double arrive_radius = 0.2;
  bool operator==(const NavigateParams& other) const {
    return target == other.target && arrive_radius == other.arrive_radius;
  }
};

struct ScanParams {
  bool operator==(const ScanParams&) const {
    return true;
  }
};

struct OutpostParams {
  Side target_side = Side::kEnemy;
  bool operator==(const OutpostParams& other) const {
    return target_side == other.target_side;
  }
};

struct RespawnParams {
  bool operator==(const RespawnParams&) const {
    return true;
  }
};

struct HealParams {
  // own_base_supply / own_outpost_buff / enemy_outpost_buff
  std::string heal_point = "own_base_supply";
  bool claim_free_ammo = true;
  bool operator==(const HealParams& other) const {
    return heal_point == other.heal_point && claim_free_ammo == other.claim_free_ammo;
  }
};

struct ResourceParams {
  std::string exchange_point;
  int amount = 0;
  bool operator==(const ResourceParams& other) const {
    return exchange_point == other.exchange_point && amount == other.amount;
  }
};

using TaskParams = std::variant<NavigateParams, ScanParams, OutpostParams, RespawnParams,
                                HealParams, ResourceParams>;

struct TaskCall {
  TaskId task;
  TaskParams params;
  Duration timeout{0};  // 0 = 不超时

  bool operator==(const TaskCall& other) const {
    return task == other.task && params == other.params && timeout == other.timeout;
  }
};

struct TaskPlan {
  std::uint64_t id = 0;
  TimePoint created_at{};
  Duration valid_for{0};
  std::optional<TaskCall> call;
  std::string rationale;
};

struct TaskSnapshot {
  TaskId task;
  std::optional<TaskCall> call;  // 当前任务的完整调用，供 Task Selection 继续评估
  TaskStatus status = TaskStatus::kIdle;
  TaskState task_state = TaskState::kIdle;
  std::string phase;
  float progress = 0.0f;
  TimePoint started_at{};
  Duration elapsed{0};
  bool can_abort = true;
  std::string abort_block_reason;
  std::string detail;
  std::map<std::string, double> metrics;
};

struct TaskResult {
  TaskId task;
  std::optional<TaskCall> call;  // 完整调用，供连续性/同点迟滞
  TaskStatus status = TaskStatus::kIdle;  // 仅 kSuccess / kFailure / kTimeout / kAborted
  TimePoint finished_at{};
  Duration elapsed{0};
  std::string reason;
  std::map<std::string, double> metrics;
};

// 一次 tick 的输出。Task 不直接写 DecisionOutput，由 TaskRuntime 统一投影。
struct TaskOutput {
  TaskStatus status = TaskStatus::kRunning;
  TaskState task_state = TaskState::kIdle;
  std::string phase;
  float progress = 0.0f;
  std::string detail;
  std::map<std::string, double> metrics;
  std::optional<Point2D> nav_goal;
  std::optional<ActionRequest> action;
};

class Task {
 public:
  virtual ~Task() = default;

  virtual bool can_start(const WorldState& world) const = 0;
  virtual void start(const TaskCall& call, TimePoint now) = 0;
  virtual TaskOutput tick(const WorldState& world, TimePoint now) = 0;
  virtual bool can_abort() const = 0;
  virtual void abort() = 0;
  virtual TaskState task_state() const = 0;
};

}  // namespace sentry_decision
