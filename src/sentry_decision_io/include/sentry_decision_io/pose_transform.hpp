#pragma once

#include <cmath>

#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision_io {

// map->odom 的二维刚体变换（平移 + 偏航）。
struct MapToOdom {
  double x = 0.0;
  double y = 0.0;
  double yaw = 0.0;
};

// 把 odom 系的自身状态变换到 map 系：p_map = T(map->odom) * p_odom。
// 位置与线速度随变换旋转并平移，角速度不变；stamp / valid 原样保留。
// 纯函数，不依赖 ROS，便于单测。
inline sentry_decision::SelfState pose_to_map(const MapToOdom& transform,
                                              const sentry_decision::SelfState& odom) {
  const double c = std::cos(transform.yaw);
  const double s = std::sin(transform.yaw);
  sentry_decision::SelfState out = odom;
  out.pose.x = transform.x + c * odom.pose.x - s * odom.pose.y;
  out.pose.y = transform.y + s * odom.pose.x + c * odom.pose.y;
  out.pose.yaw = transform.yaw + odom.pose.yaw;
  out.vx = c * odom.vx - s * odom.vy;
  out.vy = s * odom.vx + c * odom.vy;
  return out;
}

}  // namespace sentry_decision_io
