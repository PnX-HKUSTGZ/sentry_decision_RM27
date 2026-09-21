#pragma once

#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 战略层结论：本 tick 的战术模式（即决策目标）。
// P2.1 用 TacticalMode 表达目标；若后续需要更细的目标（如具体建筑 / 点位）再扩展。
struct StrategicDecision {
  TacticalMode mode = TacticalMode::kUnknown;
};

// 战略层接口：输入世界状态，输出战术模式。
//
// 纯逻辑、无副作用、可换实现（状态机 / 效用打分 / 学习模型），不依赖 ROS；
// 由组合根在行为树 tick 之前求值，经 DecisionContext::apply_strategy 写入。
class StrategicPolicy {
 public:
  virtual ~StrategicPolicy() = default;
  virtual StrategicDecision decide(const WorldState& world) const = 0;
};

}  // namespace sentry_decision
