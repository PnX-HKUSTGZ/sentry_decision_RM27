#pragma once

#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 注册 nav 模块（导航技能节点）。
void register_nav_nodes(BT::BehaviorTreeFactory& factory);

// Node:         EmitNavGoalFromPoint
// Category:     Action (synchronous, writes one Intent)
// Purpose:      按命名点解析坐标并请求导航目标（技能层去点行为）。
// Inputs:       point: string (端口, 命名点，如 "home"；可来自 SubTree 端口)
// Blackboard:   read: context(config.points)  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kNavGoal 意图。
// See:          tree/skill/goto_named_point.xml
class EmitNavGoalFromPoint : public BT::SyncActionNode {
 public:
  EmitNavGoalFromPoint(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

}  // namespace sentry_decision
