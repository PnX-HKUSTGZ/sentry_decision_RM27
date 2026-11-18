#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

enum class ReplanReason {
  kPeriodic,
  kTaskFinished,
  kTaskFailed,
  kTaskTimeout,
  kTaskAborted,
  kEvent,
  kDirective,
  kManual,
  kPlanExpired,
};

struct DecisionEpoch {
  ReplanReason reason = ReplanReason::kPeriodic;
  TimePoint at{};
  std::optional<TaskId> task;
};

// Task 工厂表。Runtime 只按 TaskId 创建任务，不直接依赖具体 Task 头文件。
class TaskRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Task>()>;

  void register_task(const TaskId& id, Factory factory);
  std::unique_ptr<Task> create(const TaskId& id) const;
  bool contains(const TaskId& id) const;

 private:
  std::map<TaskId, Factory> factories_;
};

// 有状态的 Task 执行器。负责应用 TaskPlan、tick、超时、abort、结果与决策时机。
// 不选任务、不做字段级仲裁；安全层仍可在外部强制覆盖。
class TaskRuntime {
 public:
  explicit TaskRuntime(TaskRegistry& registry);

  // 应用一个计划。过期计划只产生 kPlanExpired；不可中止的 active Task 会
  // 把计划暂存，等任务到达可中止阶段后再应用。
  void apply(const TaskPlan& plan, TimePoint now);

  // 推进 active Task。任务成功 / 失败 / 超时 / abort 后产生一个 TaskResult
  // 和一个 DecisionEpoch。
  void tick(const WorldState& world, TimePoint now);

  std::optional<TaskSnapshot> snapshot() const;
  std::optional<Point2D> nav_goal() const;
  std::optional<ActionRequest> action() const;
  std::optional<TaskResult> take_result();
  std::optional<DecisionEpoch> take_epoch();
  TaskState task_state() const;

 private:
  struct ActiveTask {
    std::unique_ptr<Task> task;
    TaskCall call;
    TimePoint started_at{};
    TaskSnapshot snapshot;
    TaskOutput output;
  };

  void start(const TaskCall& call, TimePoint now);
  void finish(TaskStatus status, TimePoint now, const std::string& reason,
              const std::map<std::string, double>& metrics);
  void apply_pending(TimePoint now);
  void push_epoch(ReplanReason reason, TimePoint now, const std::optional<TaskId>& task);

  TaskRegistry& registry_;
  std::unique_ptr<ActiveTask> active_;
  std::optional<TaskPlan> pending_plan_;
  std::optional<TaskResult> last_result_;
  std::deque<DecisionEpoch> epochs_;
};

}  // namespace sentry_decision
