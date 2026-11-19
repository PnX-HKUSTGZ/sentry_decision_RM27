#include "sentry_decision_core/resource_controller.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>
#include <utility>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr int kAmmoExchangeCap = 1000;
constexpr int kAmmoExchangeUnit = 10;

std::string point_name(std::string_view name) {
  return std::string(name);
}

Duration seconds_to_duration(double seconds) {
  return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(seconds));
}

}  // namespace

ResourceController::ResourceController(const PolicyConfig& config) : config_(config) {
  if (const auto value = config_.flag("resource.enabled")) {
    enabled_ = *value;
  }
  if (const auto value = config_.number("resource.low_ammo")) {
    low_ammo_ = std::max(0, static_cast<int>(*value));
  }
  if (const auto value = config_.number("resource.batch")) {
    batch_ = std::max(kAmmoExchangeUnit, static_cast<int>(*value));
  }
  if (const auto value = config_.number("resource.reserve_coins")) {
    reserve_coins_ = std::max(0, static_cast<int>(*value));
  }
  if (const auto value = config_.number("resource.dwell_s")) {
    dwell_ = seconds_to_duration(std::max(0.0, *value));
  }
  if (const auto value = config_.number("resource.retry_s")) {
    retry_ = seconds_to_duration(std::max(0.0, *value));
  }
  batch_ = (batch_ / kAmmoExchangeUnit) * kAmmoExchangeUnit;
}

std::optional<ResourceController::ExchangePoint> ResourceController::find_exchange_point(
    const WorldState& world) const {
  if (!world.self.valid) {
    return std::nullopt;
  }

  auto in_zone = [&](const std::string& name) -> std::optional<ExchangePoint> {
    const Point2D* point = config_.find_point(name);
    const auto radius = config_.point_radius.find(name);
    if (point == nullptr || radius == config_.point_radius.end() || radius->second <= 0.0) {
      return std::nullopt;
    }
    const double distance =
        std::hypot(world.self.pose.x - point->x, world.self.pose.y - point->y);
    if (distance > radius->second) {
      return std::nullopt;
    }
    return ExchangePoint{name, radius->second};
  };

  const std::string base(point_name(points2027::kOwnBaseSupply));
  if (world.upstream.event.supply_zone_occupied) {
    if (auto point = in_zone(base)) {
      return point;
    }
  }

  const std::string own(point_name(points2027::kOwnOutpostBuff));
  if (world.upstream.our_outpost_control == OutpostControl::kOwnFull &&
      world.upstream.our_outpost_hp > 0 &&
      EventCode::own_active(world.upstream.event.our_outpost_buff_status)) {
    if (auto point = in_zone(own)) {
      return point;
    }
  }

  const std::string enemy(point_name(points2027::kEnemyOutpostBuff));
  if (world.upstream.enemy_outpost_control == OutpostControl::kOwnFull &&
      world.upstream.enemy_outpost_hp > 0 &&
      EventCode::own_active(world.upstream.event.enemy_outpost_buff_status)) {
    if (auto point = in_zone(enemy)) {
      return point;
    }
  }
  return std::nullopt;
}

bool ResourceController::free_supply_ammo_pending(const WorldState& world) const {
  const auto duration = config_.number("match.duration_s");
  if (!duration.has_value()) {
    return false;
  }
  const int elapsed =
      std::max(0, static_cast<int>(duration.value()) - world.upstream.game_time_remaining);
  return world.supply_claimed_minutes < elapsed / 60;
}

int ResourceController::choose_amount(const WorldState& world, int max_amount) const {
  const int remaining = std::max(0, kAmmoExchangeCap - world.upstream.info1.ammo_exchanged_local);
  const int by_cap = (remaining / kAmmoExchangeUnit) * kAmmoExchangeUnit;
  const int by_coins = std::max(0, world.upstream.coins - reserve_coins_);
  int amount = std::min({max_amount, by_cap, by_coins});
  amount = (amount / kAmmoExchangeUnit) * kAmmoExchangeUnit;
  return amount;
}

void ResourceController::clear_active() {
  task_.reset();
  call_.reset();
  snapshot_.reset();
  action_.reset();
}

void ResourceController::start_resource(const ExchangePoint& point, int amount,
                                        const WorldState& world, TimePoint now) {
  if (amount < kAmmoExchangeUnit) {
    return;
  }
  task_ = std::make_unique<ResourceTask>();
  call_ = TaskCall{"Resource", ResourceParams{point.name, amount}, Duration{0}};
  if (!task_->can_start(world)) {
    clear_active();
    retry_after_ = now + retry_;
    return;
  }
  task_->start(*call_, now);
  started_at_ = now;
  snapshot_ = TaskSnapshot{};
  snapshot_->task = call_->task;
  snapshot_->call = call_;
  snapshot_->status = TaskStatus::kRunning;
  snapshot_->task_state = TaskState::kResource;
  snapshot_->started_at = now;
  tick_resource(world, now);
}

void ResourceController::tick_resource(const WorldState& world, TimePoint now) {
  if (!task_ || !call_ || !snapshot_) {
    return;
  }
  const TaskOutput out = task_->tick(world, now);
  snapshot_->status = out.status;
  snapshot_->phase = out.phase;
  snapshot_->progress = out.progress;
  snapshot_->detail = out.detail;
  snapshot_->metrics = out.metrics;
  snapshot_->elapsed = std::chrono::duration_cast<Duration>(now - started_at_);
  action_ = out.action;
  if (out.status == TaskStatus::kRunning) {
    return;
  }

  TaskResult result;
  result.task = call_->task;
  result.status = out.status;
  result.finished_at = now;
  result.elapsed = snapshot_->elapsed;
  result.reason = out.detail;
  result.metrics = out.metrics;
  result_ = std::move(result);
  clear_active();
  retry_after_ = now + retry_;
  in_zone_ = false;
  zone_name_.clear();
}

void ResourceController::tick(const WorldState& world, TimePoint now) {
  action_.reset();

  if (!enabled_ || !world.upstream.valid || !world.self.valid) {
    reset();
    return;
  }
  if (world.upstream.game_status != GameStatus::kRunning || world.upstream.self_hp <= 0) {
    reset();
    return;
  }

  if (task_) {
    tick_resource(world, now);
    return;
  }

  const auto point = find_exchange_point(world);
  if (!point.has_value()) {
    in_zone_ = false;
    zone_name_.clear();
    return;
  }

  if (!in_zone_ || point->name != zone_name_) {
    in_zone_ = true;
    zone_name_ = point->name;
    zone_since_ = now;
  }
  if (now - zone_since_ < dwell_) {
    return;
  }
  if (now < retry_after_) {
    return;
  }
  if (world.upstream.self_ammo > low_ammo_) {
    return;
  }
  // 基地还有未领取的免费发弹批次时，先让主栏 Heal 领取；不要花金币重复买。
  if (point->name == point_name(points2027::kOwnBaseSupply) &&
      free_supply_ammo_pending(world)) {
    return;
  }

  const int amount = choose_amount(world, batch_);
  if (amount < kAmmoExchangeUnit) {
    return;
  }
  start_resource(*point, amount, world, now);
}

std::optional<TaskResult> ResourceController::take_result() {
  if (!result_.has_value()) {
    return std::nullopt;
  }
  TaskResult result = std::move(*result_);
  result_.reset();
  return result;
}

void ResourceController::reset() {
  if (task_) {
    task_->abort();
  }
  clear_active();
  result_.reset();
  retry_after_ = TimePoint{};
  zone_since_ = TimePoint{};
  zone_name_.clear();
  in_zone_ = false;
}

}  // namespace sentry_decision
