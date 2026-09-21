#pragma once

#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 注册本模块全部行为树节点。
void register_sentry_nodes(BT::BehaviorTreeFactory& factory);

// =============================================================================
// Node:         IfLowHp
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断己方血量是否低于配置阈值，用于撤退分支。
// Inputs:       hp_key: string (端口, 配置 key，如 "nav.retreat_hp")
// Outputs:      -
// Blackboard:   read: context(world.referee.self_hp, valid, config)  write: (none)
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: none
// See:          tree/mission/nav/retreat.xml
// =============================================================================
class IfLowHp : public BT::SyncActionNode {
 public:
  IfLowHp(const std::string& name, const BT::NodeConfig& config);
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
// See:          tree/mission/nav/retreat.xml
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
// Purpose:      以显式坐标请求一个导航目标（测试 / 调试用）。
// Inputs:       x: double, y: double (端口)
// Outputs:      -
// Blackboard:   read: context  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kNavGoal 意图。
// See:          test/test_nodes.cpp
// =============================================================================
class EmitNavGoal : public BT::SyncActionNode {
 public:
  EmitNavGoal(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// =============================================================================
// Node:         EmitNavGoalFromPoint
// Category:     Action (synchronous, writes one Intent)
// Purpose:      按命名点解析坐标并请求导航目标（技能层去点行为）。
// Inputs:       point: string (端口, 命名点，如 "home"；可来自 SubTree 端口)
// Outputs:      -
// Blackboard:   read: context(config.points)  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kNavGoal 意图。
// See:          tree/skill/goto_named_point.xml
// =============================================================================
class EmitNavGoalFromPoint : public BT::SyncActionNode {
 public:
  EmitNavGoalFromPoint(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

}  // namespace sentry_decision
