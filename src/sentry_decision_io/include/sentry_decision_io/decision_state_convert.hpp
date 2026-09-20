#pragma once

#include <cstdint>

#include "sentry_decision_core/arbiter.hpp"
#include "sentry_decision_core/world_state.hpp"
#include "sentry_decision_msgs/msg/decision_output.hpp"
#include "sentry_decision_msgs/msg/decision_state.hpp"
#include "sentry_decision_msgs/msg/world_state.hpp"

namespace sentry_decision_io {

// core 数据契约到对外消息的纯转换：不依赖 rclcpp，可脱离节点单测。

using DecisionOutputMsg = sentry_decision_msgs::msg::DecisionOutput;
using DecisionStateMsg = sentry_decision_msgs::msg::DecisionState;
using WorldStateMsg = sentry_decision_msgs::msg::WorldState;

DecisionOutputMsg to_msg(const sentry_decision::DecisionOutput& output);
WorldStateMsg to_msg(const sentry_decision::WorldState& world);

// 把仲裁结果与信念快照填入 DecisionState（不含 header，由发布方补时间戳）。
void fill_state(DecisionStateMsg* msg, const sentry_decision::WorldState& world,
                const sentry_decision::ArbiterResult& result, std::uint32_t tick);

}  // namespace sentry_decision_io
