#pragma once

#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 注册 common 模块（条件 + 通用 Intent 叶子节点）。
void register_common_nodes(BT::BehaviorTreeFactory& factory);

// =============================================================================
// Node:         IfLowHp
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断己方血量是否低于配置阈值，用于撤退分支。
// Inputs:       hp_key: string (端口, 配置 key，如 "nav.retreat_hp")
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
// Node:         IfTacticalMode
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断本 tick 战略层给出的战术模式是否等于期望值，用于任务选择。
// Inputs:       mode: int (端口, TacticalMode 枚举值)
// Blackboard:   read: context(context.strategy.mode)  write: (none)
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: none
// See:          tree/mission/nav/retreat.xml
// =============================================================================
class IfTacticalMode : public BT::SyncActionNode {
 public:
  IfTacticalMode(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// =============================================================================
// Node:         IfEnemyOutpostDead
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断敌方前哨是否已被击毁（用于转去中央高地）。
// Inputs:       -
// Blackboard:   read: context(world.referee.valid, enemy_outpost_hp)  write: (none)
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: none
// See:          tree/mission/nav/highland.xml
// =============================================================================
class IfEnemyOutpostDead : public BT::SyncActionNode {
 public:
  IfEnemyOutpostDead(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// =============================================================================
// Node:         EmitTacticalMode
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求切换战术模式（调试 / 手动用；常规由 StrategicPolicy 产出）。
// Inputs:       mode: int (端口, TacticalMode 枚举值)
// Blackboard:   read: context  write: context.intents
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 向本 tick 的意图缓冲写入一条 kTacticalMode 意图。
// See:          test/test_nodes.cpp
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

}  // namespace sentry_decision
