#pragma once

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"
#include "sentry_interfaces/msg/decision_ack.hpp"
#include "sentry_interfaces/msg/decision_command.hpp"
#include "sentry_interfaces/msg/game_info.hpp"
#include "sentry_interfaces/msg/radar_info.hpp"
#include "sentry_interfaces/msg/sentry_info_offline.hpp"
#include "sentry_interfaces/msg/sentry_info_online.hpp"
#include "sentry_interfaces/msg/team_info.hpp"

namespace sentry_decision_io {

// sentry_interfaces 与 core 数据契约之间的纯转换：不依赖 rclcpp，可脱离节点单测。
// 上行消息在此合并进 RefereeState；有效期仍由 WorldModel 按 stamp 判定。

using GameInfoMsg = sentry_interfaces::msg::GameInfo;
using SentryInfoOnlineMsg = sentry_interfaces::msg::SentryInfoOnline;
using SentryInfoOfflineMsg = sentry_interfaces::msg::SentryInfoOffline;
using TeamInfoMsg = sentry_interfaces::msg::TeamInfo;
using RadarInfoMsg = sentry_interfaces::msg::RadarInfo;

void merge(const GameInfoMsg& msg, sentry_decision::RefereeState* out);
void merge(const SentryInfoOnlineMsg& msg, sentry_decision::RefereeState* out);
void merge(const SentryInfoOfflineMsg& msg, sentry_decision::RefereeState* out);
void merge(const TeamInfoMsg& msg, sentry_decision::RefereeState* out);
void merge(const RadarInfoMsg& msg, sentry_decision::RefereeState* out);

// 下行动作 -> DecisionCommand（header 由调用方补）。
sentry_interfaces::msg::DecisionCommand to_msg(const sentry_decision::DecisionAction& action);

}  // namespace sentry_decision_io
