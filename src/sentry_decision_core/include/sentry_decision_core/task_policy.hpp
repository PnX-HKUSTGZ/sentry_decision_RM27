#pragma once

#include <memory>
#include <optional>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/task.hpp"

namespace sentry_decision {

// 只保留上一次任务结果，供动作层做短程连续性判断。
struct HistoryView {
  std::optional<TaskResult> previous_task;
};

// Task Selection：纯函数，不持有运行时状态。
class TaskPolicy {
 public:
  virtual ~TaskPolicy() = default;
  virtual TaskPlan decide(const WorldState& world, const std::optional<TaskSnapshot>& active_task,
                          const HistoryView& history) const = 0;
};

// 第一版手写规则策略：先跑通 TaskRuntime，再替换为 utility AI。
class RuleBasedTaskPolicy : public TaskPolicy {
 public:
  explicit RuleBasedTaskPolicy(const PolicyConfig& config);

  TaskPlan decide(const WorldState& world, const std::optional<TaskSnapshot>& active_task,
                  const HistoryView& history) const override;

 private:
  const PolicyConfig& config_;
};

// 根据 config 的 strategy.use_utility 选择 rule / utility。
std::unique_ptr<TaskPolicy> make_task_policy(const PolicyConfig& config);

}  // namespace sentry_decision
