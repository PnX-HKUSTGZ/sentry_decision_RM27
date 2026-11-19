#include "sentry_decision_core/tasks/heal_task.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <variant>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr Duration kSupplyAmmoWindow{5000};

std::string point_name(std::string_view name) {
  return std::string(name);
}

}  // namespace

HealTask::HealTask(const PolicyConfig& config, int max_hp, Duration match_duration)
    : config_(config), max_hp_(max_hp), match_duration_(match_duration) {}

bool HealTask::can_start(const WorldState& world) const {
  return world.self.valid && world.upstream.valid;
}

void HealTask::start(const TaskCall& call, TimePoint now) {
  started_ = true;
  invalid_ = false;
  invalid_reason_.clear();
  target_ = nullptr;
  zone_ = ZoneKind::kBaseSupply;
  zone_radius_ = 0.0;
  ammo_wait_started_ = false;
  ammo_wait_since_ = now;

  const auto* params = std::get_if<HealParams>(&call.params);
  if (params == nullptr) {
    invalid_ = true;
    invalid_reason_ = "Heal 参数类型不匹配";
    return;
  }
  params_ = *params;

  const bool base = params_.heal_point == point_name(points2027::kOwnBaseSupply);
  const bool own_outpost = params_.heal_point == point_name(points2027::kOwnOutpostBuff);
  const bool enemy_outpost = params_.heal_point == point_name(points2027::kEnemyOutpostBuff);
  if (!base && !own_outpost && !enemy_outpost) {
    invalid_ = true;
    invalid_reason_ = "Heal 点必须是 own_base_supply、own_outpost_buff 或 enemy_outpost_buff";
    return;
  }
  if (base) {
    zone_ = ZoneKind::kBaseSupply;
  } else if (own_outpost) {
    zone_ = ZoneKind::kOwnOutpost;
  } else {
    zone_ = ZoneKind::kEnemyOutpost;
  }

  target_ = config_.find_point(params_.heal_point);
  if (target_ == nullptr) {
    invalid_ = true;
    invalid_reason_ = "命名点不存在: " + params_.heal_point;
    return;
  }
  const auto radius_it = config_.point_radius.find(params_.heal_point);
  if (radius_it == config_.point_radius.end() || radius_it->second <= 0.0) {
    invalid_ = true;
    invalid_reason_ = "Heal 点缺少区域半径: " + params_.heal_point;
  }
  if (radius_it != config_.point_radius.end()) {
    zone_radius_ = radius_it->second;
  }
}

TaskOutput HealTask::tick(const WorldState& world, TimePoint now) {
  TaskOutput out;
  out.task_state = TaskState::kHeal;
  if (!started_ || invalid_ || target_ == nullptr || max_hp_ <= 0) {
    out.status = TaskStatus::kFailure;
    out.detail = invalid_reason_.empty() ? "Heal 未启动或缺少 max_hp" : invalid_reason_;
    return out;
  }
  if (!world.self.valid || !world.upstream.valid) {
    out.status = TaskStatus::kFailure;
    out.detail = "自身 / 裁判状态无效";
    return out;
  }

  if (zone_ == ZoneKind::kOwnOutpost &&
      world.upstream.our_outpost_control != OutpostControl::kOwnFull) {
    out.status = TaskStatus::kFailure;
    out.detail = "己方前哨站未完全控制";
    return out;
  }
  if (zone_ == ZoneKind::kEnemyOutpost &&
      world.upstream.enemy_outpost_control != OutpostControl::kOwnFull) {
    out.status = TaskStatus::kFailure;
    out.detail = "敌方前哨站未完全控制";
    return out;
  }
  // 前哨站被击毁后buff失效，即使控制状态仍是完全控制也不能回血。
  if (zone_ == ZoneKind::kOwnOutpost && world.upstream.our_outpost_hp <= 0) {
    out.status = TaskStatus::kFailure;
    out.detail = "己方前哨站已被击毁";
    return out;
  }
  if (zone_ == ZoneKind::kEnemyOutpost && world.upstream.enemy_outpost_hp <= 0) {
    out.status = TaskStatus::kFailure;
    out.detail = "敌方前哨站已被击毁";
    return out;
  }

  const double distance =
      std::hypot(world.self.pose.x - target_->x, world.self.pose.y - target_->y);
  out.metrics["distance"] = distance;
  out.metrics["hp"] = static_cast<double>(world.upstream.self_hp);
  out.metrics["in_zone"] = distance <= zone_radius_ ? 1.0 : 0.0;
  out.metrics["supply_claimed_minutes"] = static_cast<double>(world.supply_claimed_minutes);

  if (distance > zone_radius_) {
    // 离开目标区域后，5 s 领取确认窗口重新计时；否则短暂离开再回来会
    // 沿用上一次的计时，导致提前失败。
    ammo_wait_started_ = false;
    if (world.nav.failed) {
      out.status = TaskStatus::kFailure;
      out.detail = "导航失败";
      return out;
    }
    out.phase = "move";
    out.nav_goal = *target_;
    out.status = TaskStatus::kRunning;
    return out;
  }

  out.phase = "wait";
  out.nav_goal = *target_;
  if (world.upstream.self_hp < max_hp_) {
    out.status = TaskStatus::kRunning;
    return out;
  }

  if (zone_ != ZoneKind::kBaseSupply || !params_.claim_free_ammo) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "血量已满";
    return out;
  }

  const int match_seconds = static_cast<int>(match_duration_.count() / 1000);
  const int elapsed_seconds = std::max(0, match_seconds - world.upstream.game_time_remaining);
  // 每满 1 分钟累积 100 发；entitled_minutes 是当前已经累积的分钟批次数。
  // supply_claimed_minutes 是已确认领取到的最高分钟批次（0 = 尚未领取）。
  const int entitled_minutes = elapsed_seconds / 60;
  if (world.supply_claimed_minutes >= entitled_minutes) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "血量已满，免费发弹已领取";
    return out;
  }

  if (!ammo_wait_started_) {
    ammo_wait_started_ = true;
    ammo_wait_since_ = now;
  }
  if (now - ammo_wait_since_ >= kSupplyAmmoWindow) {
    out.status = TaskStatus::kFailure;
    out.detail = "基地补给区免费发弹 5 s 内未到账";
    return out;
  }

  out.status = TaskStatus::kRunning;
  return out;
}

void HealTask::abort() {
  started_ = false;
  ammo_wait_started_ = false;
  target_ = nullptr;
}

}  // namespace sentry_decision
