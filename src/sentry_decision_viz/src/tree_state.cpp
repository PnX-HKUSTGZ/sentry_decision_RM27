#include "sentry_decision_viz/tree_state.hpp"

#include <set>
#include <string>
#include <utility>

namespace sentry_decision_viz {

sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree, std::uint32_t tick,
                                                          double tick_ms) {
  sentry_decision_msgs::msg::TreeStatus status;
  status.tick = tick;
  status.tick_ms = tick_ms;

  // 只收集 RUNNING 节点即可得到 active path，原因见头文件。
  std::set<std::string> active;  // set 同时完成去重与排序
  tree.applyVisitor([&](const BT::TreeNode* node) {
    sentry_decision_msgs::msg::TreeNodeStatus entry;
    entry.full_path = node->fullPath();
    entry.registration_name = node->registrationName();
    entry.instance_name = node->name();
    entry.status = static_cast<std::uint8_t>(node->status());
    if (node->status() == BT::NodeStatus::RUNNING) {
      active.insert(entry.full_path);
    }
    status.nodes.push_back(std::move(entry));
  });

  status.active_path.assign(active.begin(), active.end());
  return status;
}

}  // namespace sentry_decision_viz
