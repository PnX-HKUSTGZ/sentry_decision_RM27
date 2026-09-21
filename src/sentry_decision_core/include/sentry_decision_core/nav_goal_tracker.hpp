#pragma once

#include <optional>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 导航目标跟随契约：把仲裁后不断重复的 nav_goal 转成「发送 / 取消 / 不变」的边沿动作。
//
// 目的：目标不变时不重发（避免每 tick 重发导致无法到达）；目标变化时立即改派；
// 目标被撤销（halt / 安全急停）时取消。纯逻辑、无 ROS，可宿主单测。
class NavGoalTracker {
 public:
  enum class Decision {
    kNone,    // 无需动作
    kSend,    // 下发新目标
    kCancel,  // 取消当前目标
  };

  struct Step {
    Decision decision = Decision::kNone;
    std::optional<Point2D> goal;
  };

  Step update(const std::optional<Point2D>& goal);
  void reset();

  const std::optional<Point2D>& current() const {
    return last_goal_;
  }

 private:
  static bool same(const Point2D& a, const Point2D& b);

  std::optional<Point2D> last_goal_;
};

}  // namespace sentry_decision
