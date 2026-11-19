#include "sentry_decision_core/task_runner.hpp"

#include <algorithm>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/task_factory.hpp"

namespace sentry_decision {
namespace {
constexpr Duration kPlanValidFor{1000};
constexpr Duration kDecisionPeriod{1000};
}  // namespace

TaskRunner::TaskRunner(const PolicyConfig& config, int max_hp, Duration match_duration)
    : runtime_(registry_), policy_(make_task_policy(config)), resource_(config),
      match_duration_(match_duration) {
  register_default_tasks(registry_, config, max_hp, match_duration);
}

void TaskRunner::tick(const WorldState& world, TimePoint now) {
  WorldState view = world;
  view.supply_claimed_minutes = supply_claimed_minutes_;

  if (world.upstream.valid) {
    if (!belief_initialized_) {
      belief_initialized_ = true;
      last_self_ammo_ = world.upstream.self_ammo;
      last_local_exchange_ = world.upstream.info1.ammo_exchanged_local;
    } else if (world.upstream.game_status != GameStatus::kRunning) {
      last_self_ammo_ = world.upstream.self_ammo;
      last_local_exchange_ = world.upstream.info1.ammo_exchanged_local;
      supply_claimed_minutes_ = 0;
      // 上一局的任务结果不能影响新一局的 Resource 选择。
      history_.previous_task.reset();
    } else if (world.upstream.event.supply_zone_occupied &&
               world.upstream.self_ammo > last_self_ammo_ &&
               world.upstream.info1.ammo_exchanged_local == last_local_exchange_) {
      const int match_seconds = static_cast<int>(match_duration_.count() / 1000);
      const int elapsed = match_seconds - world.upstream.game_time_remaining;
      if (elapsed >= 0) {
        supply_claimed_minutes_ = std::max(supply_claimed_minutes_, elapsed / 60);
      }
    }
    last_self_ammo_ = world.upstream.self_ammo;
    last_local_exchange_ = world.upstream.info1.ammo_exchanged_local;
    view.supply_claimed_minutes = supply_claimed_minutes_;
  }

  // 决策由事件 / 任务结束 / 1 s 超时触发，不再每个控制周期都调用策略。
  const bool due = decision_due_ || !runtime_.snapshot().has_value() ||
                   now - last_decision_at_ >= kDecisionPeriod || !world.upstream.valid ||
                   world.upstream.game_status != GameStatus::kRunning;
  if (due) {
    TaskPlan plan = policy_->decide(view, runtime_.snapshot(), history_);
    plan.created_at = now;
    plan.valid_for = kPlanValidFor;
    if (plan.call.has_value() && !plan.rationale.empty()) {
      const auto active = runtime_.snapshot();
      if (!active.has_value() || !active->call.has_value() || !(*active->call == *plan.call)) {
        SD_LOG_ACT("task", "计划 %s：%s", plan.call->task.c_str(), plan.rationale.c_str());
      }
    }
    runtime_.apply(plan, view, now);
    last_decision_at_ = now;
    decision_due_ = false;
  }
  runtime_.tick(view, now);
  resource_.tick(view, now);

  std::optional<TaskResult> result = runtime_.take_result();
  if (result.has_value()) {
    history_.previous_task = *result;
    last_result_ = std::move(result);
    decision_due_ = true;
  }
  while (auto epoch = runtime_.take_epoch()) {
    epochs_.push_back(*epoch);
    decision_due_ = true;
  }
}

std::optional<DecisionEpoch> TaskRunner::take_epoch() {
  if (epochs_.empty()) {
    return std::nullopt;
  }
  DecisionEpoch epoch = epochs_.front();
  epochs_.pop_front();
  return epoch;
}

std::optional<TaskResult> TaskRunner::take_result() {
  if (!last_result_.has_value()) {
    return std::nullopt;
  }
  TaskResult result = std::move(*last_result_);
  last_result_.reset();
  return result;
}

}  // namespace sentry_decision
