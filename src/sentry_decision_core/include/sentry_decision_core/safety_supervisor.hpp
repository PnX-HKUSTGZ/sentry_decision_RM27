#pragma once

#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 安全限幅；由 config/policies/<POLICY>.yaml 的 safety 段提供，源码不设默认值。
struct SafetyLimits {
  double max_vx;  // 速度限幅（m/s）
  double max_vy;
  double max_wz;          // 角速度限幅（rad/s）
  bool require_referee;   // 裁判数据无效时急停
  bool require_odometry;  // 里程计数据无效时急停
};

struct SafetyResult {
  DecisionOutput output;
  bool emergency = false;            // 是否触发急停（撤销目标 + 速度清零）
  std::vector<std::string> reasons;  // 触发原因
};

// 安全监督：位于仲裁之后，做最终限幅与急停兜底，保证安全不被绕过。
// 纯逻辑、无 ROS，可脱离行为树单测。
class SafetySupervisor {
 public:
  explicit SafetySupervisor(SafetyLimits limits);

  SafetyResult apply(const WorldState& world, const DecisionOutput& input) const;

 private:
  SafetyLimits limits_;
};

}  // namespace sentry_decision
