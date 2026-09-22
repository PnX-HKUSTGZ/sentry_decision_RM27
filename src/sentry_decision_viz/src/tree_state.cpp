#include "sentry_decision_viz/tree_state.hpp"

#include <set>
#include <string>
#include <utility>

namespace sentry_decision_viz {
namespace {

// 把一次状态迁移折叠为面板可见状态，规则见头文件。
std::uint8_t visible_status(BT::NodeStatus prev, BT::NodeStatus next) {
  if (next != BT::NodeStatus::IDLE) {
    return static_cast<std::uint8_t>(next);
  }
  if (prev == BT::NodeStatus::SUCCESS || prev == BT::NodeStatus::FAILURE) {
    return static_cast<std::uint8_t>(prev);
  }
  return static_cast<std::uint8_t>(BT::NodeStatus::IDLE);
}

sentry_decision_msgs::msg::TreeStatus collect_impl(
    const BT::Tree& tree, const std::function<std::uint8_t(const BT::TreeNode&)>& status_of,
    std::uint32_t tick, double tick_ms) {
  sentry_decision_msgs::msg::TreeStatus status;
  status.tick = tick;
  status.tick_ms = tick_ms;

  // active path 取可见状态为 RUNNING 的节点：BT 中运行中节点的祖先必然也在运行，
  // 因此「RUNNING 及其祖先链」等价于「RUNNING 集合」。用于前端高亮当前执行分支。
  std::set<std::string> active;  // set 同时完成去重与排序
  tree.applyVisitor([&](const BT::TreeNode* node) {
    sentry_decision_msgs::msg::TreeNodeStatus entry;
    entry.full_path = node->fullPath();
    entry.registration_name = node->registrationName();
    entry.instance_name = node->name();
    entry.status = status_of(*node);
    if (entry.status == static_cast<std::uint8_t>(BT::NodeStatus::RUNNING)) {
      active.insert(entry.full_path);
    }
    status.nodes.push_back(std::move(entry));
  });

  status.active_path.assign(active.begin(), active.end());
  return status;
}

}  // namespace

TreeStatusRecorder::TreeStatusRecorder(BT::Tree& tree) {
  tree.applyVisitor([&](BT::TreeNode* node) {
    last_status_[node->UID()] = static_cast<std::uint8_t>(BT::NodeStatus::IDLE);
    subscriptions_.push_back(node->subscribeToStatusChange(
        [this](auto, const BT::TreeNode& changed, BT::NodeStatus prev, BT::NodeStatus next) {
          std::lock_guard<std::mutex> lock(mutex_);
          last_status_[changed.UID()] = visible_status(prev, next);
        }));
  });
}

std::uint8_t TreeStatusRecorder::status_of(const BT::TreeNode& node) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = last_status_.find(node.UID());
  return it == last_status_.end() ? static_cast<std::uint8_t>(node.status()) : it->second;
}

sentry_decision_msgs::msg::TreeStatus collect_tree_status(
    const BT::Tree& tree, const std::function<std::uint8_t(const BT::TreeNode&)>& status_of,
    std::uint32_t tick, double tick_ms) {
  return collect_impl(tree, status_of, tick, tick_ms);
}

sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree,
                                                          const TreeStatusRecorder& recorder,
                                                          std::uint32_t tick, double tick_ms) {
  return collect_impl(
      tree, [&recorder](const BT::TreeNode& node) { return recorder.status_of(node); }, tick,
      tick_ms);
}

sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree, std::uint32_t tick,
                                                          double tick_ms) {
  return collect_impl(
      tree, [](const BT::TreeNode& node) { return static_cast<std::uint8_t>(node.status()); }, tick,
      tick_ms);
}

}  // namespace sentry_decision_viz
