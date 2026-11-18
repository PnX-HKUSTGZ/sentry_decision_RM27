#include "sentry_decision_core/heal_task.hpp"

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
  zone_radius_ = 0.0;
  in_zone_ = false;
  in_zone_since_ = now;
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
  const bool outpost = params_.heal_point == point_name(points2027::kOwnOutpostBuff);
  if (!base && !outpost) {
    invalid_ = true;
    invalid_reason_ = "Heal 点必须是 own_base_supply 或 own_outpost_buff";
    return;
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

  const bool base = params_.heal_point == point_name(points2027::kOwnBaseSupply);
  const bool outpost = params_.heal_point == point_name(points2027::kOwnOutpostBuff);
  if (outpost && world.upstream.our_outpost_control != OutpostControl::kOwnFull) {
    out.status = TaskStatus::kFailure;
    out.detail = "己方前哨站未完全控制";
    return out;
  }

  const double distance =
      std::hypot(world.self.pose.x - target_->x, world.self.pose.y - target_->y);
  out.metrics["distance"] = distance;
  out.metrics["hp"] = static_cast<double>(world.upstream.self_hp);
  out.metrics["in_zone"] = distance <= zone_radius_ ? 1.0 : 0.0;
  out.metrics["supply_claimed_minutes"] = static_cast<double>(world.supply_claimed_minutes);

  if (distance > zone_radius_) {
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

  if (!in_zone_) {
    in_zone_ = true;
    in_zone_since_ = now;
  }

  out.phase = "wait";
  out.nav_goal = *target_;
  if (world.upstream.self_hp < max_hp_) {
    out.status = TaskStatus::kRunning;
    return out;
  }

  if (!base || !params_.claim_free_ammo) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "血量已满";
    return out;
  }

  const int match_seconds = static_cast<int>(match_duration_.count() / 1000);
  const int elapsed_seconds = std::max(0, match_seconds - world.upstream.game_time_remaining);
  const int current_minute = elapsed_seconds / 60;
  if (world.supply_claimed_minutes >= current_minute) {
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
  in_zone_ = false;
  ammo_wait_started_ = false;
  target_ = nullptr;
}

}  // namespace sentry_decision
