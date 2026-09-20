#include "sentry_decision_io/ros_io_node.hpp"

#include <cmath>
#include <exception>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_io/sentry_bridge.hpp"

namespace sentry_decision_io {

RosIoNode::RosIoNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("sentry_decision_io", options) {
  map_frame_ = declare_parameter<std::string>("map_frame", "map");
  const auto game_info_topic =
      declare_parameter<std::string>("game_info_topic", "/sentry/game_info");
  const auto online_info_topic =
      declare_parameter<std::string>("online_info_topic", "/sentry/online_info");
  const auto offline_info_topic =
      declare_parameter<std::string>("offline_info_topic", "/sentry/offline_info");
  const auto team_info_topic =
      declare_parameter<std::string>("team_info_topic", "/sentry/team_info");
  const auto radar_info_topic =
      declare_parameter<std::string>("radar_info_topic", "/sentry/radar_info");
  const auto decision_ack_topic =
      declare_parameter<std::string>("decision_ack_topic", "/sentry/decision_ack");
  const auto decision_command_topic =
      declare_parameter<std::string>("decision_command_topic", "/sentry/decision_command");
  const auto odom_topic = declare_parameter<std::string>("odom_topic", "/aft_mapped_to_init");
  const auto cmd_vel_topic = declare_parameter<std::string>("cmd_vel_topic", "cmd_vel");
  const auto navigate_action =
      declare_parameter<std::string>("navigate_action", "navigate_to_pose");

  game_info_sub_ = create_subscription<sentry_interfaces::msg::GameInfo>(
      game_info_topic, 10, [this](sentry_interfaces::msg::GameInfo::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
      });

  online_info_sub_ = create_subscription<sentry_interfaces::msg::SentryInfoOnline>(
      online_info_topic, 10, [this](sentry_interfaces::msg::SentryInfoOnline::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
      });

  offline_info_sub_ = create_subscription<sentry_interfaces::msg::SentryInfoOffline>(
      offline_info_topic, 10, [this](sentry_interfaces::msg::SentryInfoOffline::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
      });

  team_info_sub_ = create_subscription<sentry_interfaces::msg::TeamInfo>(
      team_info_topic, 10, [this](sentry_interfaces::msg::TeamInfo::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
      });

  radar_info_sub_ = create_subscription<sentry_interfaces::msg::RadarInfo>(
      radar_info_topic, 10, [this](sentry_interfaces::msg::RadarInfo::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
      });

  decision_ack_sub_ = create_subscription<sentry_interfaces::msg::DecisionAck>(
      decision_ack_topic, 10, [this](sentry_interfaces::msg::DecisionAck::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(decision_mutex_);
        last_ack_ = *msg;
        if (msg->accepted) {
          SD_LOG_ACT("io", "决策动作已执行 request_id=%u", msg->request_id);
        } else {
          SD_LOG_WARN("io", "决策动作被拒绝 request_id=%u code=%u", msg->request_id,
                      static_cast<unsigned>(msg->code));
        }
      });

  odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      odom_topic, 10, [this](nav_msgs::msg::Odometry::SharedPtr msg) {
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
  decision_pub_ =
      create_publisher<sentry_interfaces::msg::DecisionCommand>(decision_command_topic, 10);
  nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, navigate_action);
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

std::optional<sentry_interfaces::msg::DecisionAck> RosIoNode::last_ack() const {
  std::lock_guard<std::mutex> lock(decision_mutex_);
  return last_ack_;
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

void RosIoNode::send_action(const sentry_decision::DecisionAction& action) {
  auto msg = to_msg(action);
  msg.header.stamp = now();
  decision_pub_->publish(msg);
}

}  // namespace sentry_decision_io
