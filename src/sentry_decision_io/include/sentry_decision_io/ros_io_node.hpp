#pragma once

#include <geometry_msgs/msg/twist.hpp>
#include <memory>
#include <mutex>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <string>

#include "sentry_decision_core/io.hpp"
#include "sentry_interfaces/msg/decision_ack.hpp"
#include "sentry_interfaces/msg/decision_command.hpp"
#include "sentry_interfaces/msg/game_info.hpp"
#include "sentry_interfaces/msg/radar_info.hpp"
#include "sentry_interfaces/msg/sentry_info_offline.hpp"
#include "sentry_interfaces/msg/sentry_info_online.hpp"
#include "sentry_interfaces/msg/team_info.hpp"

namespace sentry_decision_io {

// 单一 ROS 节点，实现 core 的 IO 端口接口。
//
// 上行：订阅 sentry_interfaces 的 5 个裁判消息与 DecisionAck，回调只做解码/合并，
// 决策线程通过 referee()/odometry() 取带时间戳的快照。
// 下行：发布 DecisionCommand，并保留 /cmd_vel 与 Nav2 action。
class RosIoNode : public rclcpp::Node,
                  public sentry_decision::RefereeSource,
                  public sentry_decision::OdometrySource,
                  public sentry_decision::NavigationSink,
                  public sentry_decision::ChassisSink,
                  public sentry_decision::DecisionSink {
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

  void send_action(const sentry_decision::DecisionAction& action) override;

  // 最近一次收到的执行回执；无则空。
  std::optional<sentry_interfaces::msg::DecisionAck> last_ack() const;

 private:
  std::string map_frame_;

  mutable std::mutex referee_mutex_;
  sentry_decision::RefereeState referee_;

  mutable std::mutex odometry_mutex_;
  sentry_decision::SelfState odometry_;

  mutable std::mutex nav_mutex_;
  sentry_decision::NavState nav_state_;
  GoalHandle::SharedPtr goal_handle_;

  mutable std::mutex decision_mutex_;
  std::optional<sentry_interfaces::msg::DecisionAck> last_ack_;

  rclcpp::Subscription<sentry_interfaces::msg::GameInfo>::SharedPtr game_info_sub_;
  rclcpp::Subscription<sentry_interfaces::msg::SentryInfoOnline>::SharedPtr online_info_sub_;
  rclcpp::Subscription<sentry_interfaces::msg::SentryInfoOffline>::SharedPtr offline_info_sub_;
  rclcpp::Subscription<sentry_interfaces::msg::TeamInfo>::SharedPtr team_info_sub_;
  rclcpp::Subscription<sentry_interfaces::msg::RadarInfo>::SharedPtr radar_info_sub_;
  rclcpp::Subscription<sentry_interfaces::msg::DecisionAck>::SharedPtr decision_ack_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::DecisionCommand>::SharedPtr decision_pub_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
};

}  // namespace sentry_decision_io
