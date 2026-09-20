#include "sentry_decision_io/decision_state_publisher.hpp"

#include "sentry_decision_io/decision_state_convert.hpp"

namespace sentry_decision_io {

DecisionStatePublisher::DecisionStatePublisher(rclcpp::Node& node, const std::string& state_topic,
                                               const std::string& world_topic)
    : node_(node) {
  state_pub_ = node_.create_publisher<sentry_decision_msgs::msg::DecisionState>(state_topic, 10);
  world_pub_ = node_.create_publisher<sentry_decision_msgs::msg::WorldState>(world_topic, 10);
}

void DecisionStatePublisher::publish(const sentry_decision::WorldState& world,
                                     const sentry_decision::ArbiterResult& result,
                                     std::uint32_t tick) {
  const auto stamp = node_.now();
  sentry_decision_msgs::msg::DecisionState state;
  state.header.stamp = stamp;
  fill_state(&state, world, result, tick);
  state_pub_->publish(state);

  sentry_decision_msgs::msg::WorldState world_msg = to_msg(world);
  world_msg.header.stamp = stamp;
  world_pub_->publish(world_msg);
}

}  // namespace sentry_decision_io
