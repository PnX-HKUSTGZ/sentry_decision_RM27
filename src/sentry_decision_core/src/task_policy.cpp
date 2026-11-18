#include "sentry_decision_core/task_policy.hpp"

#include <cmath>
#include <utility>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr Duration kNavigateTimeout{30000};
constexpr Duration kPlanValidFor{1000};
constexpr int kResourceBatch = 100;
constexpr int kAmmoExchangeUnit = 10;

TaskCall call(TaskId id, TaskParams params, Duration timeout = Duration{0}) {
  return TaskCall{std::move(id), std::move(params), timeout};
}

TaskPlan plan_with(const WorldState& world, std::optional<TaskCall> call_value) {
  TaskPlan plan;
  plan.created_at = world.stamp;
  plan.valid_for = kPlanValidFor;
  plan.call = std::move(call_value);
  return plan;
}

bool point_near(const WorldState& world, const PolicyConfig& config, const std::string& name) {
  const Point2D* point = config.find_point(name);
  if (point == nullptr || !world.self.valid) {
    return false;
  }
  double radius = 0.2;
  const auto it = config.point_radius.find(name);
  if (it != config.point_radius.end() && it->second > 0.0) {
    radius = it->second;
  }
  return std::hypot(world.self.pose.x - point->x, world.self.pose.y - point->y) <= radius;
}

bool low_threshold(const PolicyConfig& config, const char* key, int value) {
  const auto threshold = config.number(key);
  return threshold.has_value() && static_cast<double>(value) <= threshold.value();
}

}  // namespace

RuleBasedTaskPolicy::RuleBasedTaskPolicy(const PolicyConfig& config) : config_(config) {}

TaskPlan RuleBasedTaskPolicy::decide(const WorldState& world,
                                     const std::optional<TaskSnapshot>& active_task,
                                     const HistoryView& history) const {
  if (!world.upstream.valid || world.upstream.game_status != GameStatus::kRunning) {
    return plan_with(world, std::nullopt);
  }

  if (world.upstream.self_hp <= 0) {
    return plan_with(world, call("Respawn", RespawnParams{}));
  }

  if (low_threshold(config_, "nav.retreat_hp", world.upstream.self_hp) ||
      low_threshold(config_, "nav.low_ammo", world.upstream.self_ammo)) {
    return plan_with(world,
                     call("Heal", HealParams{std::string(points2027::kOwnBaseSupply), true}));
  }

  // 继续当前任务：除死亡 / 低血 / 低弹外，不在任务未结束时换任务。
  // 任务结束后 Runtime 清空 active，下一拍再按规则选下一个。
  if (active_task.has_value() && active_task->status == TaskStatus::kRunning &&
      active_task->call.has_value()) {
    return plan_with(world, *active_task->call);
  }

  const bool previous_heal = history.previous_task.has_value() &&
                             history.previous_task->task == "Heal" &&
                             history.previous_task->status == TaskStatus::kSuccess;
  const bool at_base = point_near(world, config_, std::string(points2027::kOwnBaseSupply));
  const int remaining_exchange = 1000 - world.upstream.info1.ammo_exchanged_local;
  if (previous_heal && at_base && remaining_exchange >= kResourceBatch &&
      world.upstream.coins >= kResourceBatch && kResourceBatch % kAmmoExchangeUnit == 0) {
    return plan_with(world, call("Resource", ResourceParams{std::string(points2027::kOwnBaseSupply),
                                                            kResourceBatch}));
  }

  if (world.upstream.enemy_outpost_hp > 0 &&
      world.upstream.enemy_outpost_control == OutpostControl::kNotOwned) {
    const std::string point(points2027::kEnemyOutpostFront);
    if (config_.find_point(point) != nullptr) {
      if (point_near(world, config_, point)) {
        return plan_with(world, call("Outpost", OutpostParams{Side::kEnemy}));
      }
      return plan_with(world, call("Navigate", NavigateParams{point, 0.2}, kNavigateTimeout));
    }
  }

  return plan_with(world, call("Scan", ScanParams{}));
}

}  // namespace sentry_decision
