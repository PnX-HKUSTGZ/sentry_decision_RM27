#include "sentry_decision_io/ros_io_node.hpp"

#include <cmath>
#include <exception>

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision_io {

RosIoNode::RosIoNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("sentry_decision_io", options) {
  map_frame_ = declare_parameter<std::string>("map_frame", "map");
  const auto health_topic = declare_parameter<std::string>("health_topic", "/ifhealth");
  const auto ammo_topic = declare_parameter<std::string>("ammo_topic", "/remain_ammo");
  const auto base_health_topic =
      declare_parameter<std::string>("base_health_topic", "/our_base_health");
  const auto our_outpost_topic =
      declare_parameter<std::string>("our_outpost_topic", "/our_outpost_health");
  const auto enemy_outpost_topic =
      declare_parameter<std::string>("enemy_outpost_topic", "/enemy_outpost_health");
  const auto can_rebuild_topic =
      declare_parameter<std::string>("can_rebuild_topic", "/can_rebuild_outpost");
  const auto odom_topic = declare_parameter<std::string>("odom_topic", "/odom");
  const auto cmd_vel_topic = declare_parameter<std::string>("cmd_vel_topic", "cmd_vel");
  const auto navigate_action =
      declare_parameter<std::string>("navigate_action", "navigate_to_pose");
  const auto set_bool_service = declare_parameter<std::string>("set_bool_service", "/set_bool");

  subscribe_u16(health_topic, &sentry_decision::RefereeState::self_hp, true);
  subscribe_u16(ammo_topic, &sentry_decision::RefereeState::self_ammo, false);
  subscribe_u16(base_health_topic, &sentry_decision::RefereeState::base_hp, false);
  subscribe_u16(our_outpost_topic, &sentry_decision::RefereeState::our_outpost_hp, false);
  subscribe_u16(enemy_outpost_topic, &sentry_decision::RefereeState::enemy_outpost_hp, false);

  can_rebuild_sub_ = create_subscription<std_msgs::msg::Bool>(
      can_rebuild_topic, 10, [this](const std_msgs::msg::Bool::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        referee_.can_rebuild_outpost = msg->data;
        referee_.stamp = sentry_decision::SteadyClock::now();
      });

  odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      odom_topic, 10, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(odometry_mutex_);
        odometry_.pose.x = msg->pose.pose.position.x;
        odometry_.pose.y = msg->pose.pose.position.y;
        const auto& q = msg->pose.pose.orientation;
        odometry_.pose.yaw =
            std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
        odometry_.vx = msg->twist.twist.linear.x;
        odometry_.vy = msg->twist.twist.linear.y;
        odometry_.wz = msg->twist.twist.angular.z;
        odometry_.stamp = sentry_decision::SteadyClock::now();
        odometry_.valid = true;
      });

  cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(cmd_vel_topic, 10);
  nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, navigate_action);
  set_bool_client_ = create_client<std_srvs::srv::SetBool>(set_bool_service);
}

void RosIoNode::subscribe_u16(const std::string& topic,
                              int sentry_decision::RefereeState::* field, bool mark_valid) {
  referee_subs_.push_back(create_subscription<std_msgs::msg::UInt16>(
      topic, 10, [this, field, mark_valid](const std_msgs::msg::UInt16::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        referee_.*field = static_cast<int>(msg->data);
        referee_.stamp = sentry_decision::SteadyClock::now();
        if (mark_valid) {
          referee_.valid = true;
        }
      }));
}

bool RosIoNode::referee(sentry_decision::RefereeState* out) const {
  std::lock_guard<std::mutex> lock(referee_mutex_);
  *out = referee_;
  return referee_.valid;
}

bool RosIoNode::odometry(sentry_decision::SelfState* out) const {
  std::lock_guard<std::mutex> lock(odometry_mutex_);
  *out = odometry_;
  return odometry_.valid;
}

void RosIoNode::send_goal(const sentry_decision::Point2D& goal) {
  {
    std::lock_guard<std::mutex> lock(nav_mutex_);
    nav_state_.current_goal = goal;
    nav_state_.stamp = sentry_decision::SteadyClock::now();
    nav_state_.valid = true;
    nav_state_.reached = false;
    nav_state_.failed = false;
  }
  if (!nav_client_->action_server_is_ready()) {
    SD_LOG_WARN("io", "导航 action server 未就绪");
    return;
  }

  NavigateToPose::Goal nav_goal;
  nav_goal.pose.header.frame_id = map_frame_;
  nav_goal.pose.header.stamp = now();
  nav_goal.pose.pose.position.x = goal.x;
  nav_goal.pose.pose.position.y = goal.y;
  nav_goal.pose.pose.orientation.w = 1.0;

  auto options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
  options.goal_response_callback = [this](const GoalHandle::SharedPtr& handle) {
    std::lock_guard<std::mutex> lock(nav_mutex_);
    goal_handle_ = handle;
  };
  options.feedback_callback = [this](GoalHandle::SharedPtr,
                                     const std::shared_ptr<const NavigateToPose::Feedback>) {
    std::lock_guard<std::mutex> lock(nav_mutex_);
    nav_state_.stamp = sentry_decision::SteadyClock::now();
  };
  options.result_callback = [this](const GoalHandle::WrappedResult& result) {
    std::lock_guard<std::mutex> lock(nav_mutex_);
    nav_state_.stamp = sentry_decision::SteadyClock::now();
    switch (result.code) {
      case rclcpp_action::ResultCode::SUCCEEDED:
        nav_state_.reached = true;
        nav_state_.failed = false;
        break;
      case rclcpp_action::ResultCode::ABORTED:
        nav_state_.reached = false;
        nav_state_.failed = true;
        break;
      case rclcpp_action::ResultCode::CANCELED:
      default:
        nav_state_.reached = false;
        nav_state_.failed = false;
        break;
    }
    goal_handle_.reset();
  };
  nav_client_->async_send_goal(nav_goal, options);
}

void RosIoNode::cancel_goal() {
  GoalHandle::SharedPtr handle;
  {
    std::lock_guard<std::mutex> lock(nav_mutex_);
    handle = goal_handle_;
    nav_state_.current_goal.reset();
    nav_state_.stamp = sentry_decision::SteadyClock::now();
  }
  if (handle) {
    nav_client_->async_cancel_goal(handle);
  }
}

sentry_decision::NavState RosIoNode::status() const {
  std::lock_guard<std::mutex> lock(nav_mutex_);
  return nav_state_;
}

void RosIoNode::set_velocity(const sentry_decision::Twist& cmd) {
  geometry_msgs::msg::Twist msg;
  msg.linear.x = cmd.vx;
  msg.linear.y = cmd.vy;
  msg.angular.z = cmd.wz;
  cmd_vel_pub_->publish(msg);
}

void RosIoNode::set_flag(const std::string& name, bool value) {
  if (!set_bool_client_->service_is_ready()) {
    SD_LOG_WARN("io", "set_bool 服务未就绪（%s）", name.c_str());
    return;
  }
  auto request = std::make_shared<std_srvs::srv::SetBool::Request>();
  request->data = value;
  set_bool_client_->async_send_request(request);
}

}  // namespace sentry_decision_io
