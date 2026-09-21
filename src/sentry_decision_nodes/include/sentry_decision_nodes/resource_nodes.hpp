#pragma once

#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 注册 resource 模块（资源 / 复活相关的条件与动作节点）。
void register_resource_nodes(BT::BehaviorTreeFactory& factory);

// Node:         IfCanFreeResurrect
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断是否可确认免费复活。
// Inputs:       -
// Blackboard:   read: context(world.referee.valid, info1.can_free_resurrect)  write: (none)
// Side Effects: none
// See:          tree/resource/root.xml -> Revive
class IfCanFreeResurrect : public BT::SyncActionNode {
 public:
  IfCanFreeResurrect(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// Node:         IfLowAmmo
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断剩余弹量是否低于配置阈值。
// Inputs:       ammo_key: string (端口, 配置 key)
// Blackboard:   read: context(world.referee.valid, self_ammo, config)  write: (none)
// Side Effects: none
// See:          tree/resource/root.xml -> AmmoExchange
class IfLowAmmo : public BT::SyncActionNode {
 public:
  IfLowAmmo(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// Node:         IfCoinsAtLeast
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断己方金币是否不少于配置阈值。
// Inputs:       coins_key: string (端口, 配置 key)
// Blackboard:   read: context(world.referee.valid, coins, config)  write: (none)
// Side Effects: none
// See:          tree/resource/root.xml
class IfCoinsAtLeast : public BT::SyncActionNode {
 public:
  IfCoinsAtLeast(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// Node:         RequestFreeRevive
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求确认免费复活。
// Inputs:       -
// Blackboard:   read: context  write: context.intents
// Side Effects: 写入一条 kResourceRequest 意图（revive = true）。
// See:          tree/resource/root.xml -> Revive
class RequestFreeRevive : public BT::SyncActionNode {
 public:
  RequestFreeRevive(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// Node:         RequestHpExchange
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求兑换血量。
// Inputs:       amount_key: string (端口, 配置 key, 兑换数量)
// Blackboard:   read: context(config)  write: context.intents
// Side Effects: 写入一条 kResourceRequest 意图（hp = 数量）。
// See:          tree/resource/root.xml -> HpExchange
class RequestHpExchange : public BT::SyncActionNode {
 public:
  RequestHpExchange(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

// Node:         RequestAmmoExchange
// Category:     Action (synchronous, writes one Intent)
// Purpose:      请求兑换发弹量。
// Inputs:       amount_key: string (端口, 配置 key, 兑换数量)
// Blackboard:   read: context(config)  write: context.intents
// Side Effects: 写入一条 kResourceRequest 意图（ammo = 数量）。
// See:          tree/resource/root.xml -> AmmoExchange
class RequestAmmoExchange : public BT::SyncActionNode {
 public:
  RequestAmmoExchange(const std::string& name, const BT::NodeConfig& config);
  static BT::PortsList providedPorts();
  BT::NodeStatus tick() override;
};

}  // namespace sentry_decision
