#include "sentry_decision_core/task_policy.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "sentry_decision_core/points_2027.hpp"
#include "sentry_decision_core/utility_task_policy.hpp"

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

bool valid_heal_point(const PolicyConfig& config, const std::string& name) {
  if (config.find_point(name) == nullptr) {
    return false;
  }
  const auto radius = config.point_radius.find(name);
  return radius != config.point_radius.end() && radius->second > 0.0;
}

bool inside_area(const WorldState& world, const PolicyConfig& config,
                 const std::string& name) {
  const Point2D* point = config.find_point(name);
  const auto radius = config.point_radius.find(name);
  if (point == nullptr || radius == config.point_radius.end() || radius->second <= 0.0 ||
      !world.self.valid) {
    return false;
  }
  return std::hypot(world.self.pose.x - point->x, world.self.pose.y - point->y) <=
         radius->second;
}

// 回血点优先级：敌方前哨 buff（中央/敌方高地区域） > 己方前哨 buff（中央/己方高地区域）
// > 基地。低弹必须回基地，因为前哨站增益点不能补弹。
std::string choose_heal_point(const WorldState& world, const PolicyConfig& config,
                              bool need_ammo) {
  const std::string base(points2027::kOwnBaseSupply);
  if (need_ammo || !world.self.valid) {
    return base;
  }

  const bool in_center = inside_area(world, config, std::string(points2027::kHighlandAreaCenter));
  const std::string enemy_buff(points2027::kEnemyOutpostBuff);
  if (world.upstream.enemy_outpost_control == OutpostControl::kOwnFull &&
      world.upstream.enemy_outpost_hp > 0 && valid_heal_point(config, enemy_buff) &&
      (in_center ||
       inside_area(world, config, std::string(points2027::kHighlandAreaEnemy)))) {
    return enemy_buff;
  }

  const std::string own_buff(points2027::kOwnOutpostBuff);
  if (world.upstream.our_outpost_control == OutpostControl::kOwnFull &&
      world.upstream.our_outpost_hp > 0 && valid_heal_point(config, own_buff) &&
      (in_center ||
       inside_area(world, config, std::string(points2027::kHighlandAreaOur)))) {
    return own_buff;
  }
  return base;
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

  // 低血优先回血：按高地范围点选择前哨站增益点，区域外或低弹回基地。
  if (low_hp) {
    const std::string heal_point = choose_heal_point(world, config_, low_ammo);
    return plan_with(world, call("Heal", HealParams{heal_point, true}));
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
  // Scan 是无限任务，不算“进行中的承诺”；每拍允许重新评估，否则新出现的前哨目标
  // 会被一直挡住。
  if (active_task.has_value() && active_task->status == TaskStatus::kRunning &&
      active_task->call.has_value() && active_task->task != "Scan") {
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

std::unique_ptr<TaskPolicy> make_task_policy(const PolicyConfig& config) {
  if (config.flag("strategy.use_utility").value_or(false)) {
    return std::make_unique<UtilityTaskPolicy>(config);
  }
  return std::make_unique<RuleBasedTaskPolicy>(config);
}

}  // namespace sentry_decision
