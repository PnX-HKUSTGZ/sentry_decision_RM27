#pragma once

#include <cstdint>
#include <rclcpp/rclcpp.hpp>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_msgs/msg/tree_status.hpp"

namespace sentry_decision_viz {

// 采集行为树状态并发布 /decision/tree_status。
// 只读：不改变树状态、不参与决策；publish 在决策 tick 线程调用。
class TreeStatePublisher {
 public:
  static constexpr const char* kDefaultTopic = "/decision/tree_status";

  explicit TreeStatePublisher(rclcpp::Node& node, const std::string& topic = kDefaultTopic);

  // 采集并发布一次快照；在 tree tickOnce 之后调用。
  void publish(const BT::Tree& tree, std::uint32_t tick, double tick_ms = 0.0);

 private:
  rclcpp::Node& node_;
  rclcpp::Publisher<sentry_decision_msgs::msg::TreeStatus>::SharedPtr publisher_;
};

}  // namespace sentry_decision_viz
