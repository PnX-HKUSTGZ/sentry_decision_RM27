#include "sentry_decision_io/rosbag_replay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/serialization.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/u_int16.hpp>

#include "sentry_decision_io/intervention_convert.hpp"
#include "sentry_decision_msgs/msg/intervention_event.hpp"

namespace sentry_decision_io {
namespace {

using sentry_decision::Duration;

template <typename T>
T deserialize(const rosbag2_storage::SerializedBagMessage& bag_msg) {
  rclcpp::SerializedMessage serialized(*bag_msg.serialized_data);
  rclcpp::Serialization<T> serialization;
  T msg;
  serialization.deserialize_message(&serialized, &msg);
  return msg;
}

sentry_decision::SelfState self_state_from_odometry(const nav_msgs::msg::Odometry& msg) {
  sentry_decision::SelfState self;
  self.pose.x = msg.pose.pose.position.x;
  self.pose.y = msg.pose.pose.position.y;
  const auto& q = msg.pose.pose.orientation;
  self.pose.yaw = std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  self.vx = msg.twist.twist.linear.x;
  self.vy = msg.twist.twist.linear.y;
  self.wz = msg.twist.twist.angular.z;
  self.valid = true;
  return self;
}

// InterventionEvent -> core 干预命令；无法重建的事件返回 false 并跳过。
bool intervention_from_msg(const sentry_decision_msgs::msg::InterventionEvent& msg,
                           sentry_decision::InterventionCommand* out) {
  using Event = sentry_decision_msgs::msg::InterventionEvent;
  using Kind = sentry_decision::InterventionCommand::Kind;
  switch (msg.kind) {
    case Event::KIND_INTENT: {
      std::string error;
      return parse_manual_override(msg.field, msg.value, msg.lease_sec, msg.reason, out, &error);
    }
    case Event::KIND_CLEAR_INTENT:
      out->kind = Kind::kClearIntent;
      return parse_intent_field(msg.field, &out->intent_field);
    case Event::KIND_WORLD_OVERRIDE:
      out->kind = Kind::kWorldOverride;
      out->world_field = static_cast<sentry_decision::WorldField>(msg.field);
      try {
        out->world_value = std::stod(msg.value);
      } catch (const std::exception&) {
        return false;
      }
      return true;
    case Event::KIND_CLEAR_WORLD:
      out->kind = Kind::kClearWorld;
      out->world_field = static_cast<sentry_decision::WorldField>(msg.field);
      return true;
    case Event::KIND_MODULE_SWITCH:
      out->kind = Kind::kModuleSwitch;
      out->module = msg.module;
      out->enabled = msg.enabled;
      return true;
    case Event::KIND_CLEAR_ALL:
      out->kind = Kind::kClearAll;
      return true;
    default:
      return false;
  }
}

}  // namespace

sentry_decision::ReplayData load_replay_data(const std::string& bag_uri,
                                             const ReplayTopics& topics) {
  rosbag2_cpp::Reader reader;
  reader.open(bag_uri);

  sentry_decision::ReplayData data;
  sentry_decision::RefereeState referee;
  std::int64_t start_ns = -1;

  while (reader.has_next()) {
    const auto bag_msg = reader.read_next();
    // 使用录制时刻（send_timestamp，rosbag2 未提供时会回退为 recv_timestamp），
    // 而不是读取时的时钟，保证回放时间轴可复现。
    if (start_ns < 0) {
      start_ns = bag_msg->send_timestamp;
    }
    const Duration at{std::max<std::int64_t>(0, (bag_msg->send_timestamp - start_ns) / 1000000)};
    const std::string& topic = bag_msg->topic_name;

    if (topic == topics.odometry) {
      data.odometry.push_back(
          {at, self_state_from_odometry(deserialize<nav_msgs::msg::Odometry>(*bag_msg))});
      continue;
    }

    if (topic == topics.interventions) {
      sentry_decision::InterventionCommand command;
      if (intervention_from_msg(deserialize<sentry_decision_msgs::msg::InterventionEvent>(*bag_msg),
                                &command)) {
        data.interventions.push_back({at, command});
      }
      continue;
    }

    if (topic == topics.can_rebuild_outpost) {
      referee.can_rebuild_outpost = deserialize<std_msgs::msg::Bool>(*bag_msg).data;
    } else if (topic == topics.self_hp) {
      referee.self_hp = static_cast<int>(deserialize<std_msgs::msg::UInt16>(*bag_msg).data);
    } else if (topic == topics.self_ammo) {
      referee.self_ammo = static_cast<int>(deserialize<std_msgs::msg::UInt16>(*bag_msg).data);
    } else if (topic == topics.base_hp) {
      referee.base_hp = static_cast<int>(deserialize<std_msgs::msg::UInt16>(*bag_msg).data);
    } else if (topic == topics.our_outpost_hp) {
      referee.our_outpost_hp = static_cast<int>(deserialize<std_msgs::msg::UInt16>(*bag_msg).data);
    } else if (topic == topics.enemy_outpost_hp) {
      referee.enemy_outpost_hp =
          static_cast<int>(deserialize<std_msgs::msg::UInt16>(*bag_msg).data);
    } else {
      continue;  // 未知话题跳过
    }

    referee.valid = true;
    data.referee.push_back({at, referee});
  }

  return data;
}

}  // namespace sentry_decision_io
