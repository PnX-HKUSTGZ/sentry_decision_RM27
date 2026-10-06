#include "sentry_decision_io/ros_io_node.hpp"

#include <tf2/exceptions.h>
#include <tf2/time.h>

#include <cmath>
#include <exception>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_io/sentry_bridge.hpp"

namespace sentry_decision_io {

namespace {

double yaw_from_quaternion(double x, double y, double z, double w) {
  return std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
}

}  // namespace

RosIoNode::RosIoNode(const rclcpp::NodeOptions& options)
    : rclcpp::Node("sentry_decision_io", options) {
  map_frame_ = declare_parameter<std::string>("map_frame", "map");
  odom_frame_ = declare_parameter<std::string>("odom_frame", "");
  const auto game_info_topic =
      declare_parameter<std::string>("game_info_topic", "/sentry/game_info");
  const auto online_info_topic =
      declare_parameter<std::string>("online_info_topic", "/sentry/online_info");
  const auto offline_info_topic =
      declare_parameter<std::string>("offline_info_topic", "/sentry/offline_info");
  const auto team_info_topic =
      declare_parameter<std::string>("team_info_topic", "/sentry/team_info");
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
        has_game_info_ = true;
      });

  online_info_sub_ = create_subscription<sentry_interfaces::msg::SentryInfoOnline>(
      online_info_topic, 10, [this](sentry_interfaces::msg::SentryInfoOnline::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(referee_mutex_);
        merge(*msg, &referee_);
        referee_.stamp = sentry_decision::SteadyClock::now();
        referee_.valid = true;
        has_online_info_ = true;
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

  decision_ack_sub_ = create_subscription<sentry_interfaces::msg::DecisionAck>(
      decision_ack_topic, 10, [this](sentry_interfaces::msg::DecisionAck::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(decision_mutex_);
        last_ack_ = *msg;
        sentry_decision::ActionAck ack;
        ack.request_id = msg->request_id;
        ack.accepted = msg->accepted;
        ack.code = msg->code;
        ack.detail = msg->detail;
        ack_queue_.push_back(std::move(ack));
        if (msg->accepted) {
          SD_LOG_ACT("io", "决策动作已执行 request_id=%u", msg->request_id);
        } else {
          SD_LOG_WARN("io", "决策动作被拒绝 request_id=%u code=%u", msg->request_id,
                      static_cast<unsigned>(msg->code));
        }
      });

  odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      odom_topic, 10, [this](nav_msgs::msg::Odometry::SharedPtr msg) {
        sentry_decision::SelfState state;
        state.pose.x = msg->pose.pose.position.x;
        state.pose.y = msg->pose.pose.position.y;
        const auto& q = msg->pose.pose.orientation;
        state.pose.yaw = yaw_from_quaternion(q.x, q.y, q.z, q.w);
        state.vx = msg->twist.twist.linear.x;
        state.vy = msg->twist.twist.linear.y;
        state.wz = msg->twist.twist.angular.z;
        state.stamp = sentry_decision::SteadyClock::now();
        state.valid = true;
        // 双仓库：odom 话题在 odom 系，需用 TF map->odom 转成 map 系。TF 未就绪时
        // 丢弃本帧并标记失效，绝不把 odom 系坐标误当 map 系。
        if (!odom_frame_.empty()) {
          MapToOdom map_to_odom;
          if (!lookup_map_to_odom(&map_to_odom)) {
            std::lock_guard<std::mutex> lock(odometry_mutex_);
            odometry_.valid = false;
            return;
          }
          state = pose_to_map(map_to_odom, state);
        }
        std::lock_guard<std::mutex> lock(odometry_mutex_);
        odometry_ = state;
      });

  cmd_vel_pub_ = create_publisher<geometry_msgs::msg::Twist>(cmd_vel_topic, 10);
  decision_pub_ =
      create_publisher<sentry_interfaces::msg::DecisionCommand>(decision_command_topic, 10);
  nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, navigate_action);

  if (!odom_frame_.empty()) {
    tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_unique<tf2_ros::TransformListener>(*tf_buffer_);
  }
}

bool RosIoNode::referee(sentry_decision::UpstreamState* out) const {
  std::lock_guard<std::mutex> lock(referee_mutex_);
  *out = referee_;
  // 只到其中一条裁判消息时，缺失字段会以默认 0 参与决策（例如被误判为「0 血 / 阵亡」），
  // 因此必须 GameInfo + SentryInfoOnline 都出现过才判有效；此后由 stamp 超时决定失效。
  return referee_.valid &&
         sentry_decision_io::referee_sources_ready(has_game_info_, has_online_info_);
}

bool RosIoNode::odometry(sentry_decision::SelfState* out) const {
  std::lock_guard<std::mutex> lock(odometry_mutex_);
  *out = odometry_;
  return odometry_.valid;
}

bool RosIoNode::lookup_map_to_odom(MapToOdom* out) {
  try {
    const geometry_msgs::msg::TransformStamped tf =
        tf_buffer_->lookupTransform(map_frame_, odom_frame_, tf2::TimePointZero);
    out->x = tf.transform.translation.x;
    out->y = tf.transform.translation.y;
    const auto& q = tf.transform.rotation;
    out->yaw = yaw_from_quaternion(q.x, q.y, q.z, q.w);
    tf_warned_ = false;
    return true;
  } catch (const tf2::TransformException& ex) {
    if (!tf_warned_) {
      SD_LOG_WARN("io", "map->%s 变换不可用，丢弃 odom: %s", odom_frame_.c_str(), ex.what());
      tf_warned_ = true;
    }
    return false;
  }
}

std::optional<sentry_interfaces::msg::DecisionAck> RosIoNode::last_ack() const {
  std::lock_guard<std::mutex> lock(decision_mutex_);
  return last_ack_;
}

std::vector<sentry_decision::ActionAck> RosIoNode::take_acks() {
  std::lock_guard<std::mutex> lock(decision_mutex_);
  std::vector<sentry_decision::ActionAck> acks;
  acks.swap(ack_queue_);
  return acks;
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
