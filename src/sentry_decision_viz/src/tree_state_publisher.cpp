#include "sentry_decision_viz/tree_state_publisher.hpp"

#include "sentry_decision_viz/tree_state.hpp"

namespace sentry_decision_viz {

TreeStatePublisher::TreeStatePublisher(rclcpp::Node& node, const std::string& topic) : node_(node) {
  // transient local：后到的网页 / Groot 能拿到最近一帧，而不是空等下一拍。
  rclcpp::QoS qos(rclcpp::KeepLast(1));
  qos.transient_local();
  publisher_ = node_.create_publisher<sentry_decision_msgs::msg::TreeStatus>(topic, qos);
}

void TreeStatePublisher::publish(const BT::Tree& tree, std::uint32_t tick, double tick_ms) {
  sentry_decision_msgs::msg::TreeStatus status = collect_tree_status(tree, tick, tick_ms);
  status.header.stamp = node_.now();
  publisher_->publish(status);
}

}  // namespace sentry_decision_viz
