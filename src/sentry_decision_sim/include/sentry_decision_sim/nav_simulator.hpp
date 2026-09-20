#pragma once

#include <optional>

#include "sentry_decision_core/io.hpp"

namespace sentry_decision_sim {

// 伪导航：维护一个虚拟机器人，接收导航目标并按固定速度朝目标移动，
// 同时作为里程计来源。这样本地调试无需启动导航仓库。
//
// 用法：每个 tick 先 update(now) 推进运动，再把本对象同时作为
// OdometrySource 与 NavigationSink 传给 WorldModel；send_goal 由决策输出驱动。
class NavSimulator : public sentry_decision::OdometrySource,
                     public sentry_decision::NavigationSink {
 public:
  explicit NavSimulator(double speed = 2.0, double tolerance = 0.2);

  void set_pose(const sentry_decision::Point2D& pose);
  // 让当前目标立即失败，用于测试降级 / 失败分支。
  void fail_current_goal();

  // OdometrySource
  bool odometry(sentry_decision::SelfState* out) const override;

  // NavigationSink
  void send_goal(const sentry_decision::Point2D& goal) override;
  void cancel_goal() override;
  sentry_decision::NavState status() const override;

  // 按 now 推进：首次调用 dt = 0，之后按与上次 now 的差值积分。
  void update(sentry_decision::TimePoint now);

  const sentry_decision::Point2D& pose() const {
    return self_.pose;
  }

 private:
  sentry_decision::SelfState self_;
  sentry_decision::NavState nav_;
  std::optional<sentry_decision::Point2D> goal_;
  sentry_decision::TimePoint last_now_{};
  bool has_last_now_ = false;
  bool reached_ = false;
  bool failed_ = false;
  double speed_;
  double tolerance_;
};

}  // namespace sentry_decision_sim
