#pragma once

#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 注册本模块全部行为树节点。
void register_sentry_nodes(BT::BehaviorTreeFactory& factory);

// =============================================================================
// Node:         CheckLowHp
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断己方血量是否低于阈值，用于撤退分支。
// Inputs:       hp_threshold: int (端口)
// Outputs:      -
// Blackboard:   read: context(world.referee.self_hp, valid)  write: (none)
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: none
// See:          tree/demo_tree.xml -> retreat
// =============================================================================
class CheckLowHp : public BT::SyncActionNode {
 public:
  CheckLowHp(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// =============================================================================
// Node:         EmitTacticalMode
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求切换战术模式（高层意图）。
// Inputs:       mode: int (端口, TacticalMode 枚举值)
// Outputs:      -
// Blackboard:   read: context  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kTacticalMode 意图。
// See:          tree/demo_tree.xml -> retreat / patrol
// =============================================================================
class EmitTacticalMode : public BT::SyncActionNode {
 public:
  EmitTacticalMode(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// =============================================================================
// Node:         EmitNavGoal
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求一个导航目标（技能层意图）。
// Inputs:       x: double, y: double (端口)
// Outputs:      -
// Blackboard:   read: context  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kNavGoal 意图。
// See:          tree/demo_tree.xml -> retreat / patrol
// =============================================================================
class EmitNavGoal : public BT::SyncActionNode {
 public:
  EmitNavGoal(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

}  // namespace sentry_decision
