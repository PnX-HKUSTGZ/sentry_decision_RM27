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

// Node:         PatrolLoop
// Category:     Action (stateful, RUNNING / 循环巡逻)
// Purpose:      按命名点列表循环巡逻：对当前点持续请求导航目标，到达后停留 dwell_key
// 秒再切换下一点。 Inputs:       points: string (端口, 逗号分隔的命名点，如
// "highland_a,highland_b,highland_c")
//               dwell_key: string (端口, 配置 key, 到达后停留秒数，如 "nav.patrol_dwell_s")
//               loop_id: string (端口, 巡逻线标识，用于区分状态；缺省用节点名)
// Blackboard:   read: context(config.points, config.numbers, world.nav.reached, world.stamp)
//               write: context.intents, context.patrol
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: 每 tick 写一条 kNavGoal 意图；按 loop_id 维护 context.patrol 游标。
// See:          tree/skill/patrol_loop.xml
class PatrolLoop : public BT::StatefulActionNode {
 public:
  PatrolLoop(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus onStart() override;
  BT::NodeStatus onRunning() override;
  void onHalted() override;
};

}  // namespace sentry_decision
