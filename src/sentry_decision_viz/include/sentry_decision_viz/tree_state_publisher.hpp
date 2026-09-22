#pragma once

#include <cstdint>
#include <rclcpp/rclcpp.hpp>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_msgs/msg/tree_status.hpp"
#include "sentry_decision_viz/tree_state.hpp"

namespace sentry_decision_viz {

// 采集行为树状态并发布 /decision/tree_status。
// 只读：不改变树状态、不参与决策；publish 在决策 tick 线程调用。
//
// 内部用 TreeStatusRecorder 缓存可见状态，避免 BT.CPP 完成 tick 后的 resetStatus
// 把整棵树刷成 IDLE（见 tree_state.hpp）。
class TreeStatePublisher {
 public:
  static constexpr const char* kDefaultTopic = "/decision/tree_status";

  TreeStatePublisher(rclcpp::Node& node, BT::Tree& tree, const std::string& topic = kDefaultTopic);

  // 采集并发布一次快照；在 tree tickOnce 之后调用。
  void publish(std::uint32_t tick, double tick_ms = 0.0);

 private:
  rclcpp::Node& node_;
  rclcpp::Publisher<sentry_decision_msgs::msg::TreeStatus>::SharedPtr publisher_;
  BT::Tree& tree_;
  TreeStatusRecorder recorder_;
};

}  // namespace sentry_decision_viz
