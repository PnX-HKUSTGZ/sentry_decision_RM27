#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_msgs/msg/tree_status.hpp"

namespace sentry_decision_viz {

// 缓存每个行为树节点的「可见状态」。
//
// BT.CPP >= 4.10 会在 tick 末尾把已完成节点重置为 IDLE：根节点完成时由
// Tree::tickRoot 调用 resetStatus()，控制节点完成时由父节点重置子节点。若在
// tickOnce() 返回后直接读取 node->status()，整棵树都会显示 IDLE，面板失去意义。
//
// 本类订阅 BT.CPP 的 status-change 信号，按如下规则维护可见状态：
//   - 迁移到非 IDLE：记录新状态；
//   - 迁移到 IDLE 且原状态为 SUCCESS / FAILURE：保留原状态（已完成的任务仍可见）；
//   - 迁移到 IDLE 且原状态为 RUNNING：记录 IDLE（被 halt 的分支不再活跃）。
// 与 BT.CPP 官方 Groot2Publisher 处理 IDLE 的思路一致。
class TreeStatusRecorder {
 public:
  explicit TreeStatusRecorder(BT::Tree& tree);

  // 返回缓存的可见状态；未记录时退回读取 node->status()。
  std::uint8_t status_of(const BT::TreeNode& node) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::uint16_t, std::uint8_t> last_status_;
  std::vector<BT::TreeNode::StatusChangeSubscriber> subscriptions_;
};

// 采集一次树快照；status_of 提供每个节点的可见状态（不传则读取实时状态）。
sentry_decision_msgs::msg::TreeStatus collect_tree_status(
    const BT::Tree& tree, const std::function<std::uint8_t(const BT::TreeNode&)>& status_of,
    std::uint32_t tick, double tick_ms = 0.0);

// 使用缓存采集，能正确显示已完成节点的 SUCCESS / FAILURE。
sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree,
                                                          const TreeStatusRecorder& recorder,
                                                          std::uint32_t tick, double tick_ms = 0.0);

// 直接读取节点实时状态，不缓存；供单测与无 tick 重置的简单场景使用。
sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree, std::uint32_t tick,
                                                          double tick_ms = 0.0);

}  // namespace sentry_decision_viz
