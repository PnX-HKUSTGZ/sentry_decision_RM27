#pragma once

#include "sentry_decision_core/io.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 输入超时阈值。
struct WorldTimeouts {
  Duration referee{500};
  Duration odometry{200};
  Duration navigation{500};
};

// 信念层：从输入源取原始值，判定新鲜度，组装 WorldState。
// 纯逻辑，不依赖 ROS，可用假实现单测；有效性变化时记录日志。
class WorldModel {
 public:
  WorldModel(RefereeSource& referee, OdometrySource& odometry, NavigationSink& navigation,
             WorldTimeouts timeouts = {});

  WorldState snapshot(TimePoint now);

 private:
  RefereeSource& referee_;
  OdometrySource& odometry_;
  NavigationSink& navigation_;
  WorldTimeouts timeouts_;
  bool referee_was_valid_ = true;
  bool odometry_was_valid_ = true;
  bool navigation_was_valid_ = true;
};

}  // namespace sentry_decision
