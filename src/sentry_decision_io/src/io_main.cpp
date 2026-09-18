#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "sentry_decision_io/ros_io_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<sentry_decision_io::RosIoNode>();
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
