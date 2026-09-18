#pragma once

#include <string>

#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// IO 端口接口：core 只依赖这些抽象，具体 real / sim / replay 实现在 io 包。

// 输入源：返回是否取到数据；数据的新鲜度由信念层判定。
class RefereeSource {
 public:
  virtual ~RefereeSource() = default;
  virtual bool referee(RefereeState* out) const = 0;
};

class OdometrySource {
 public:
  virtual ~OdometrySource() = default;
  virtual bool odometry(SelfState* out) const = 0;
};

// 导航执行端：决策只发目标 / 取消，执行端回报状态。
class NavigationSink {
 public:
  virtual ~NavigationSink() = default;
  virtual void send_goal(const Point2D& goal) = 0;
  virtual void cancel_goal() = 0;
  virtual NavState status() const = 0;
};

// 底盘执行端：决策只发速度与开关标志。
class ChassisSink {
 public:
  virtual ~ChassisSink() = default;
  virtual void set_velocity(const Twist& cmd) = 0;
  virtual void set_flag(const std::string& name, bool value) = 0;
};

}  // namespace sentry_decision
