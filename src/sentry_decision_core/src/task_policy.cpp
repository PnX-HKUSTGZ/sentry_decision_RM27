#include "sentry_decision_core/task_policy.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr Duration kNavigateTimeout{30000};
constexpr Duration kPlanValidFor{1000};

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

// 当前是否还有未领取的补给区免费发弹批次（每满 1 分钟 100 发）。
bool free_ammo_available(const WorldState& world, const PolicyConfig& config) {
  const auto duration = config.number("match.duration_s");
  if (!duration.has_value()) {
    return false;
  }
  const int elapsed =
      std::max(0, static_cast<int>(duration.value()) - world.upstream.game_time_remaining);
  return world.supply_claimed_minutes < elapsed / 60;
}

}  // namespace

RuleBasedTaskPolicy::RuleBasedTaskPolicy(const PolicyConfig& config) : config_(config) {}

TaskPlan RuleBasedTaskPolicy::decide(const WorldState& world,
                                     const std::optional<TaskSnapshot>& active_task,
                                     const HistoryView& /*history*/) const {
  if (!world.upstream.valid || world.upstream.game_status != GameStatus::kRunning) {
    return plan_with(world, std::nullopt);
  }

  if (world.upstream.self_hp <= 0) {
    return plan_with(world, call("Respawn", RespawnParams{}));
  }

  const bool low_hp = low_threshold(config_, "nav.retreat_hp", world.upstream.self_hp);
  const bool low_ammo = low_threshold(config_, "nav.low_ammo", world.upstream.self_ammo);
  const bool at_base = point_near(world, config_, std::string(points2027::kOwnBaseSupply));

  // 低血优先回血。
  if (low_hp) {
    return plan_with(world,
                     call("Heal", HealParams{std::string(points2027::kOwnBaseSupply), true}));
  }

  // 低弹但不在基地：先回基地。到基地后优先兑换，避免「Heal 已满血 -> 立刻成功 -> 再 Heal」
  // 的空转（此时任务态会反复回到 idle，且弹药永远不增加）。
  if (low_ammo && !at_base) {
    return plan_with(world,
                     call("Heal", HealParams{std::string(points2027::kOwnBaseSupply), true}));
  }
  // 低弹且在基地但还有免费发弹批次：Heal 可领取。买弹本身由独立 Resource
  // 辅助栏在后台完成，不占主栏。
  if (low_ammo && at_base && free_ammo_available(world, config_)) {
    return plan_with(world,
                     call("Heal", HealParams{std::string(points2027::kOwnBaseSupply), true}));
  }

  // 继续当前任务：除死亡 / 低血 / 低弹外，不在任务未结束时换任务。
  // 任务结束后 Runtime 清空 active，下一拍再按规则选下一个。
  if (active_task.has_value() && active_task->status == TaskStatus::kRunning &&
      active_task->call.has_value()) {
    return plan_with(world, *active_task->call);
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
