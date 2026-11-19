#pragma once

#include "sentry_decision_core/io.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 输入超时阈值；由 config/policies/<POLICY>.yaml 的 timeouts 段提供，源码不设默认值。
struct WorldTimeouts {
  Duration upstream;
  Duration odometry;
  Duration navigation;
};

// 信念层：从输入源取原始值，判定新鲜度，组装 WorldState。
// 纯逻辑，不依赖 ROS，可用假实现单测；有效性变化时记录日志。
class WorldModel {
 public:
  WorldModel(UpstreamSource& upstream, OdometrySource& odometry, NavigationSink& navigation,
             WorldTimeouts timeouts);

  WorldState snapshot(TimePoint now);

 private:
  void update_outpost_owner(OutpostOwner* owner, int* previous_hp, bool* pending,
                            TimePoint* pending_at, int hp, OutpostControl control,
                            TimePoint now);
  void update_outpost_trend(OutpostTrend* trend, int* last_hp, TimePoint* last_sample,
                            TimePoint* last_damage, bool* initialized, int hp, TimePoint now);

  UpstreamSource& upstream_;
  OdometrySource& odometry_;
  NavigationSink& navigation_;
  WorldTimeouts timeouts_;
  bool upstream_was_valid_ = true;
  bool odometry_was_valid_ = true;
  bool navigation_was_valid_ = true;

  // 前哨站建筑归属 belief：开局己方半场归己方、敌方半场归敌方；
  // 血量上升视为重建，归属反转到「另一方」，1 s 后用 full 控制方校正。
  OutpostOwner our_outpost_owner_ = OutpostOwner::kOwn;
  OutpostOwner enemy_outpost_owner_ = OutpostOwner::kEnemy;
  int previous_our_outpost_hp_ = -1;
  int previous_enemy_outpost_hp_ = -1;
  bool our_rebuild_pending_ = false;
  bool enemy_rebuild_pending_ = false;
  TimePoint our_rebuild_at_{};
  TimePoint enemy_rebuild_at_{};

  // 前哨站血量趋势：1 s 差分 + 最近受击时间；只服务策略，不进协议。
  bool our_trend_initialized_ = false;
  bool enemy_trend_initialized_ = false;
  int last_our_trend_hp_ = 0;
  int last_enemy_trend_hp_ = 0;
  TimePoint last_our_trend_sample_{};
  TimePoint last_enemy_trend_sample_{};
  TimePoint last_our_damage_at_{};
  TimePoint last_enemy_damage_at_{};
};

}  // namespace sentry_decision
