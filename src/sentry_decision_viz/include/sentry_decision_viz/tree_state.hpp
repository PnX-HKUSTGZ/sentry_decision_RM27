#pragma once

#include <cstdint>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_msgs/msg/tree_status.hpp"

namespace sentry_decision_viz {

// 只读地把一棵行为树展平成 TreeStatus 快照：全部节点状态 + active path。
// 纯函数、不依赖 ROS 节点，可在单测中直接断言。
//
// active_path 取全部 RUNNING 节点：BT 中运行中的节点的祖先必然也在运行，
// 因此「RUNNING 及其祖先链」等价于「RUNNING 集合」。用于前端高亮当前执行分支。
sentry_decision_msgs::msg::TreeStatus collect_tree_status(const BT::Tree& tree, std::uint32_t tick,
                                                          double tick_ms = 0.0);

}  // namespace sentry_decision_viz
