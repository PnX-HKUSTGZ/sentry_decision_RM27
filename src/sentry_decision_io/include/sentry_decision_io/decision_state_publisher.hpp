#pragma once

#include <cstdint>
#include <rclcpp/rclcpp.hpp>
#include <string>

#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/world_state.hpp"
#include "sentry_decision_msgs/msg/decision_state.hpp"
#include "sentry_decision_msgs/msg/world_state.hpp"

namespace sentry_decision_io {

// 发布 /decision/state 与 /decision/world_state，供可视化与 rosbag 记录。
// 只是发布者，不参与决策；可独立于 RosIoNode 使用。
class DecisionStatePublisher {
 public:
  explicit DecisionStatePublisher(rclcpp::Node& node,
                                  const std::string& state_topic = "/decision/state",
                                  const std::string& world_topic = "/decision/world_state");

  // 发布一次决策快照；header 时间戳取节点当前时间。
  void publish(const sentry_decision::WorldState& world,
               const sentry_decision::ArbiterResult& result, std::uint32_t tick);

 private:
  rclcpp::Node& node_;
  rclcpp::Publisher<sentry_decision_msgs::msg::DecisionState>::SharedPtr state_pub_;
  rclcpp::Publisher<sentry_decision_msgs::msg::WorldState>::SharedPtr world_pub_;
};

}  // namespace sentry_decision_io
