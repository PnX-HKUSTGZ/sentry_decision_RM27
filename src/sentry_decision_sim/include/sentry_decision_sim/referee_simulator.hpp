#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "sentry_decision_core/io.hpp"

namespace sentry_decision_sim {

// Dummy 裁判系统：为本地仿真提供 RefereeState，不需要真实下位机通信包。
//
// 用法：先 mutable_state() 设置初始值，再 schedule() 注册相对时间轴的动作；
// 每个 tick 调用 update(now) 执行到点脚本并刷新 stamp / valid，然后交给 WorldModel。
// ROS 无关，可在宿主机直接单测。
class RefereeSimulator : public sentry_decision::RefereeSource {
 public:
  using Action = std::function<void(sentry_decision::RefereeState&)>;

  RefereeSimulator();

  // 在相对起点 at 时刻执行一次 action；同一 at 只执行一次。
  void schedule(sentry_decision::Duration at, Action action);

  // 推进到 now：首次调用记录起点；执行到点脚本并刷新 stamp / valid。
  void update(sentry_decision::TimePoint now);

  sentry_decision::RefereeState& mutable_state() {
    return state_;
  }
  bool referee(sentry_decision::RefereeState* out) const override;

 private:
  sentry_decision::RefereeState state_;
  sentry_decision::TimePoint epoch_{};
  bool started_ = false;
  std::vector<std::pair<sentry_decision::Duration, Action>> schedule_;
  std::vector<bool> fired_;
};

}  // namespace sentry_decision_sim
