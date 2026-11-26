#include "sentry_decision_core/world_model.hpp"

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {
namespace {

bool fresh(TimePoint stamp, TimePoint now, Duration timeout) {
  return now <= stamp + timeout;
}

}  // namespace

WorldModel::WorldModel(UpstreamSource& upstream, OdometrySource& odometry,
                       NavigationSink& navigation, WorldTimeouts timeouts)
    : upstream_(upstream), odometry_(odometry), navigation_(navigation), timeouts_(timeouts) {}

void WorldModel::update_outpost_owner(OutpostOwner* owner, int* previous_hp, bool* pending,
                                      TimePoint* pending_at, int hp, OutpostControl control,
                                      TimePoint now) {
  // 前哨站不会回血；HP 上升只可能来自重建。按规则 5.6.1，重建只发生在
  // 「一方完全控制 -> 另一方完全控制」，因此归属反转。
  if (*previous_hp >= 0 && hp > *previous_hp) {
    switch (*owner) {
      case OutpostOwner::kOwn:
        *owner = OutpostOwner::kEnemy;
        break;
      case OutpostOwner::kEnemy:
        *owner = OutpostOwner::kOwn;
        break;
      case OutpostOwner::kUnknown:
      default:
        break;
    }
    *pending = true;
    *pending_at = now;
  }

  // 1 s 后若已经能看到 full 控制方，用它校正；kOwnTemporary 不校正，
  // 因为「我方临时控制」也可能对应敌方归属。
  if (*pending && now - *pending_at >= Duration{1000}) {
    if (control == OutpostControl::kOwnFull) {
      *owner = OutpostOwner::kOwn;
    } else if (control == OutpostControl::kNotOwned) {
      *owner = OutpostOwner::kEnemy;
    }
    *pending = false;
  }

  // full 控制是最强证据，随时校正。
  if (control == OutpostControl::kOwnFull) {
    *owner = OutpostOwner::kOwn;
  }
  *previous_hp = hp;
}

void WorldModel::update_outpost_trend(OutpostTrend* trend, int* last_hp, TimePoint* last_sample,
                                      TimePoint* last_damage, bool* initialized, int hp,
                                      TimePoint now) {
  if (!*initialized) {
    *initialized = true;
    *last_hp = hp;
    *last_sample = now;
    *last_damage = now - Duration{10000};
    trend->hp_delta_1s = 0;
    trend->under_attack = false;
    return;
  }

  const int delta = hp - *last_hp;
  if (now - *last_sample >= Duration{1000}) {
    trend->hp_delta_1s = delta;
    *last_hp = hp;
    *last_sample = now;
  }
  if (delta < 0) {
    *last_damage = now;
  }
  trend->under_attack = now - *last_damage < Duration{1500};
}

WorldState WorldModel::snapshot(TimePoint now) {
  WorldState state;
  state.stamp = now;

  UpstreamState upstream;
  const bool has_upstream = upstream_.upstream(&upstream);
  upstream.valid = has_upstream && upstream.valid && fresh(upstream.stamp, now, timeouts_.upstream);
  state.upstream = upstream;
  if (!upstream.valid || upstream.game_status != GameStatus::kRunning) {
    our_outpost_owner_ = OutpostOwner::kOwn;
    enemy_outpost_owner_ = OutpostOwner::kEnemy;
    previous_our_outpost_hp_ = -1;
    previous_enemy_outpost_hp_ = -1;
    our_rebuild_pending_ = false;
    enemy_rebuild_pending_ = false;
    our_trend_initialized_ = false;
    enemy_trend_initialized_ = false;
    state.our_outpost_trend = OutpostTrend{};
    state.enemy_outpost_trend = OutpostTrend{};
  } else {
    update_outpost_owner(&our_outpost_owner_, &previous_our_outpost_hp_, &our_rebuild_pending_,
                         &our_rebuild_at_, upstream.our_outpost_hp, upstream.our_outpost_control,
                         now);
    update_outpost_owner(&enemy_outpost_owner_, &previous_enemy_outpost_hp_,
                         &enemy_rebuild_pending_, &enemy_rebuild_at_, upstream.enemy_outpost_hp,
                         upstream.enemy_outpost_control, now);
    update_outpost_trend(&state.our_outpost_trend, &last_our_trend_hp_, &last_our_trend_sample_,
                         &last_our_damage_at_, &our_trend_initialized_, upstream.our_outpost_hp,
                         now);
    update_outpost_trend(&state.enemy_outpost_trend, &last_enemy_trend_hp_,
                         &last_enemy_trend_sample_, &last_enemy_damage_at_,
                         &enemy_trend_initialized_, upstream.enemy_outpost_hp, now);
  }
  state.upstream.our_outpost_owner = our_outpost_owner_;
  state.upstream.enemy_outpost_owner = enemy_outpost_owner_;
  if (upstream_was_valid_ != upstream.valid) {
    if (upstream.valid) {
      SD_LOG_ACT("world_model", "upstream data recovered");
    } else {
      SD_LOG_WARN("world_model", "upstream data stale");
    }
    upstream_was_valid_ = upstream.valid;
  }

  SelfState self;
  const bool has_odometry = odometry_.odometry(&self);
  self.valid = has_odometry && self.valid && fresh(self.stamp, now, timeouts_.odometry);
  state.self = self;
  if (odometry_was_valid_ != self.valid) {
    if (self.valid) {
      SD_LOG_ACT("world_model", "odometry recovered");
    } else {
      SD_LOG_WARN("world_model", "odometry stale");
    }
    odometry_was_valid_ = self.valid;
  }

  NavState nav = navigation_.status();
  nav.valid = nav.valid && fresh(nav.stamp, now, timeouts_.navigation);
  state.nav = nav;
  if (navigation_was_valid_ != nav.valid) {
    if (nav.valid) {
      SD_LOG_ACT("world_model", "navigation state recovered");
    } else {
      SD_LOG_WARN("world_model", "navigation state stale");
    }
    navigation_was_valid_ = nav.valid;
  }

  return state;
}

}  // namespace sentry_decision
