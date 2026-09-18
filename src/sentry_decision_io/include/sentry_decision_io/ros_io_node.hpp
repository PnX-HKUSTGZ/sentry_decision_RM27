#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <geometry_msgs/msg/twist.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/u_int16.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include "sentry_decision_core/io.hpp"

namespace sentry_decision_io {

// 单一 ROS 节点，同时实现 core 的四个 IO 端口接口。
// 订阅回调只做解码并写入带时间戳的缓存，决策线程通过 referee()/odometry() 取快照。
class RosIoNode : public rclcpp::Node,
                  public sentry_decision::RefereeSource,
                  public sentry_decision::OdometrySource,
                  public sentry_decision::NavigationSink,
                  public sentry_decision::ChassisSink {
 public:
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;

  explicit RosIoNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

  bool referee(sentry_decision::RefereeState* out) const override;
  bool odometry(sentry_decision::SelfState* out) const override;

  void send_goal(const sentry_decision::Point2D& goal) override;
  void cancel_goal() override;
  sentry_decision::NavState status() const override;

  void set_velocity(const sentry_decision::Twist& cmd) override;
  void set_flag(const std::string& name, bool value) override;

 private:
  void subscribe_u16(const std::string& topic, int sentry_decision::RefereeState::* field,
                     bool mark_valid);

  std::string map_frame_;

  mutable std::mutex referee_mutex_;
  sentry_decision::RefereeState referee_;

  mutable std::mutex odometry_mutex_;
  sentry_decision::SelfState odometry_;

  mutable std::mutex nav_mutex_;
  sentry_decision::NavState nav_state_;
  GoalHandle::SharedPtr goal_handle_;

  std::vector<rclcpp::Subscription<std_msgs::msg::UInt16>::SharedPtr> referee_subs_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr can_rebuild_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr set_bool_client_;
};

}  // namespace sentry_decision_io
