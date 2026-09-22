#include "sentry_decision_io/sentry_bridge.hpp"

#include "sentry_decision_core/referee_protocol.hpp"

namespace sentry_decision_io {

void merge(const GameInfoMsg& msg, sentry_decision::RefereeState* out) {
  out->game_status = sentry_decision::decode_game_status(msg.game_status);
  out->game_time_remaining = msg.game_time_remaining;
  out->coins = msg.coin_remaining;
  out->event = sentry_decision::decode_event_code(msg.event_code);
  out->detect_color = msg.detect_color;
  out->can_rebuild_outpost = msg.can_rebuild_outpost;
  out->manual_point.x = msg.manual_point_x;
  out->manual_point.y = msg.manual_point_y;
  out->manual_key = msg.manual_key;
  out->enemy_outpost_hp = msg.enemy_outpost_hp;
  out->enemy_base_hp = msg.enemy_base_hp;
}

void merge(const SentryInfoOnlineMsg& msg, sentry_decision::RefereeState* out) {
  out->self_hp = msg.self_health;
  out->self_ammo = msg.bullets_remaining;
  out->cooling_value = msg.cooling_value;
  out->heat_limit = msg.heat_limit;
  out->current_heat = msg.current_heat;
  out->energy_ratio = msg.energy_ratio;
  out->gimbal_yaw_deg = msg.speed_monitor_angle;
  out->info1 = sentry_decision::decode_sentry_info1(msg.sentry_info_1);
  out->info2 = sentry_decision::decode_sentry_info2(msg.sentry_info_2);
  out->info3 = sentry_decision::decode_sentry_info3(msg.sentry_info_3);
}

void merge(const SentryInfoOfflineMsg& msg, sentry_decision::RefereeState* out) {
  out->lifter_pos = msg.lifter_current_pos;
  out->is_transformable = msg.is_transformable;
  out->transform_state = msg.transform_state;
  out->capacitor_capacity = msg.capacitor_capacity;
  out->tunnel_yaw_aligned = msg.tunnel_yaw_aligned;
  out->yaw_camera_to_gimbal = msg.yaw_camerainit_to_gimbal;
}

void merge(const TeamInfoMsg& msg, sentry_decision::RefereeState* out) {
  out->base_hp = msg.base_hp;
  out->our_outpost_hp = msg.outpost_hp;
}

void merge(const RadarInfoMsg& msg, sentry_decision::RefereeState* out) {
  out->enemy_coins = msg.enemy_coin_left;
  out->enemy_coins_accumulated = msg.enemy_coin_accumulated;
  out->enemy_outpost_sensed = msg.is_enemy_outpost_sensed;
}

namespace {

std::uint8_t to_msg_kind(sentry_decision::DecisionActionKind kind) {
  using K = sentry_decision::DecisionActionKind;
  switch (kind) {
    case K::kAmmoExchange:
      return sentry_interfaces::msg::DecisionAction::KIND_AMMO_EXCHANGE;
    case K::kHpExchange:
      return sentry_interfaces::msg::DecisionAction::KIND_HP_EXCHANGE;
    case K::kFreeResurrect:
      return sentry_interfaces::msg::DecisionAction::KIND_FREE_RESURRECT;
    case K::kInstantResurrect:
      return sentry_interfaces::msg::DecisionAction::KIND_INSTANT_RESURRECT;
    case K::kRemoteAmmoExchange:
      return sentry_interfaces::msg::DecisionAction::KIND_REMOTE_AMMO_EXCHANGE;
    case K::kRemoteHpExchange:
      return sentry_interfaces::msg::DecisionAction::KIND_REMOTE_HP_EXCHANGE;
    case K::kNone:
    default:
      return sentry_interfaces::msg::DecisionAction::KIND_NONE;
  }
}

std::uint8_t to_msg_mode(sentry_decision::ActionMode mode) {
  return mode == sentry_decision::ActionMode::kPolled
             ? sentry_interfaces::msg::DecisionAction::MODE_POLLED
             : sentry_interfaces::msg::DecisionAction::MODE_ONE_SHOT;
}

}  // namespace

sentry_interfaces::msg::DecisionCommand to_msg(const sentry_decision::DecisionAction& action) {
  sentry_interfaces::msg::DecisionCommand msg;
  msg.request_id = action.request_id;
  msg.action.kind = to_msg_kind(action.kind);
  msg.action.mode = to_msg_mode(action.mode);
  msg.action.interval_ms = static_cast<std::uint16_t>(action.interval.count());
  msg.action.value = action.value;
  return msg;
}

}  // namespace sentry_decision_io
