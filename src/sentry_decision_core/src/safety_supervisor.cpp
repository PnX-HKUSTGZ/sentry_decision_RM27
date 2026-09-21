#include "sentry_decision_core/safety_supervisor.hpp"

#include <algorithm>

namespace sentry_decision {
namespace {

double clamp(double value, double limit) {
  return std::max(-limit, std::min(limit, value));
}

}  // namespace

SafetySupervisor::SafetySupervisor(SafetyLimits limits) : limits_(limits) {}

SafetyResult SafetySupervisor::apply(const WorldState& world, const DecisionOutput& input) const {
  SafetyResult result;
  result.output = input;

  if (limits_.require_referee && !world.referee.valid) {
    result.reasons.push_back("referee 数据无效");
  }
  if (limits_.require_odometry && !world.self.valid) {
    result.reasons.push_back("里程计数据无效");
  }
  result.emergency = !result.reasons.empty();

  if (result.emergency) {
    // 急停：撤销导航目标并清零速度（即使此前未下发速度）。
    result.output.nav_goal.reset();
    result.output.cmd_vel = Twist{};
    return result;
  }

  if (result.output.cmd_vel.has_value()) {
    Twist& cmd = *result.output.cmd_vel;
    cmd.vx = clamp(cmd.vx, limits_.max_vx);
    cmd.vy = clamp(cmd.vy, limits_.max_vy);
    cmd.wz = clamp(cmd.wz, limits_.max_wz);
  }
  return result;
}

}  // namespace sentry_decision
