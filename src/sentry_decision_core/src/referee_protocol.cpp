#include "sentry_decision_core/referee_protocol.hpp"

namespace sentry_decision {
namespace {

// 从 raw 中取 [shift, shift + width) 位。
std::uint32_t bits(std::uint64_t raw, unsigned shift, unsigned width) {
  return static_cast<std::uint32_t>((raw >> shift) & ((1ull << width) - 1ull));
}

}  // namespace

GameStatus decode_game_status(std::uint8_t raw) {
  switch (raw) {
    case 0:
      return GameStatus::kNotStarted;
    case 1:
      return GameStatus::kPreparation;
    case 2:
      return GameStatus::kSelfCheck;
    case 3:
      return GameStatus::kCountdown;
    case 4:
      return GameStatus::kRunning;
    case 5:
      return GameStatus::kSettling;
    default:
      return GameStatus::kUnknown;
  }
}

SentryStance decode_stance(std::uint8_t raw) {
  switch (raw) {
    case 1:
      return SentryStance::kAttack;
    case 2:
      return SentryStance::kDefense;
    case 3:
      return SentryStance::kMove;
    default:
      return SentryStance::kUnknown;
  }
}

namespace {

OutpostControl decode_outpost_control(std::uint32_t raw) {
  switch (raw) {
    case 0:
      return OutpostControl::kOwnFull;
    case 1:
      return OutpostControl::kOwnTemporary;
    default:
      return OutpostControl::kNotOwned;
  }
}

}  // namespace

EventCode decode_event_code(std::uint32_t event_data, std::uint32_t event_data_2,
                            TeamColor own_color) {
  const bool red = own_color == TeamColor::kRed;
  const std::uint8_t r_fort = static_cast<std::uint8_t>(bits(event_data, 5, 3));
  const std::uint8_t b_fort = static_cast<std::uint8_t>(bits(event_data, 8, 3));
  const std::uint8_t r_base = static_cast<std::uint8_t>(bits(event_data, 11, 3));
  const std::uint8_t b_base = static_cast<std::uint8_t>(bits(event_data, 14, 3));
  const std::uint8_t r_central = static_cast<std::uint8_t>(bits(event_data, 18, 2));
  const std::uint8_t b_central = static_cast<std::uint8_t>(bits(event_data, 20, 2));
  const std::uint8_t r_outpost_buff = static_cast<std::uint8_t>(bits(event_data, 22, 3));
  const std::uint8_t b_outpost_buff = static_cast<std::uint8_t>(bits(event_data, 25, 3));
  const std::uint8_t r_outpost_ctrl = static_cast<std::uint8_t>(bits(event_data, 28, 2));
  const std::uint8_t b_outpost_ctrl = static_cast<std::uint8_t>(bits(event_data, 30, 2));

  EventCode out;
  out.supply_zone_occupied = bits(event_data, 0, 1) != 0;
  out.small_energy_status = static_cast<std::uint8_t>(bits(event_data, 1, 2));
  out.big_energy_status = static_cast<std::uint8_t>(bits(event_data, 3, 2));
  out.own_fort_buff_status = red ? r_fort : b_fort;
  out.enemy_fort_buff_status = red ? b_fort : r_fort;
  out.own_base_buff_status = red ? r_base : b_base;
  out.enemy_base_buff_status = red ? b_base : r_base;
  out.high_point_buff_active = bits(event_data, 17, 1) != 0;
  out.own_central_highland_status = red ? r_central : b_central;
  out.enemy_central_highland_status = red ? b_central : r_central;
  out.our_outpost_buff_status = red ? r_outpost_buff : b_outpost_buff;
  out.enemy_outpost_buff_status = red ? b_outpost_buff : r_outpost_buff;
  out.our_outpost_control = decode_outpost_control(red ? r_outpost_ctrl : b_outpost_ctrl);
  out.enemy_outpost_control = decode_outpost_control(red ? b_outpost_ctrl : r_outpost_ctrl);
  out.enemy_dart_last_hit_time_s = static_cast<std::uint16_t>(bits(event_data_2, 4, 9));
  out.enemy_dart_last_target = static_cast<std::uint8_t>(bits(event_data_2, 13, 3));
  out.base_buff_occupied = EventCode::own_active(out.own_base_buff_status);
  return out;
}

SentryInfo1 decode_sentry_info1(std::uint32_t raw) {
  SentryInfo1 out;
  out.ammo_exchanged_local = static_cast<std::uint16_t>(bits(raw, 0, 11));
  out.remote_ammo_exchange_count = static_cast<std::uint8_t>(bits(raw, 11, 4));
  out.remote_health_exchange_count = static_cast<std::uint8_t>(bits(raw, 15, 4));
  out.can_free_resurrect = bits(raw, 19, 1) != 0;
  out.can_instant_resurrect = bits(raw, 20, 1) != 0;
  out.instant_resurrect_cost = static_cast<std::uint16_t>(bits(raw, 21, 10));
  return out;
}

SentryInfo2 decode_sentry_info2(std::uint16_t raw) {
  SentryInfo2 out;
  out.disengaged = bits(raw, 0, 1) != 0;
  out.remaining_ammo_exchange = static_cast<std::uint16_t>(bits(raw, 1, 11));
  out.stance = decode_stance(static_cast<std::uint8_t>(bits(raw, 12, 2)));
  out.can_activate_energy = bits(raw, 14, 1) != 0;
  out.stance_enhanced = bits(raw, 15, 1) != 0;
  return out;
}

SentryInfo3 decode_sentry_info3(std::uint64_t raw) {
  SentryInfo3 out;
  out.attack_stance_remaining_s = static_cast<std::uint8_t>(bits(raw, 0, 8));
  out.defense_stance_remaining_s = static_cast<std::uint8_t>(bits(raw, 8, 8));
  out.move_stance_remaining_s = static_cast<std::uint8_t>(bits(raw, 16, 8));
  out.attack_enhanced_remaining_s = static_cast<std::uint8_t>(bits(raw, 32, 8));
  out.defense_enhanced_remaining_s = static_cast<std::uint8_t>(bits(raw, 40, 8));
  out.move_enhanced_remaining_s = static_cast<std::uint8_t>(bits(raw, 48, 8));
  return out;
}

}  // namespace sentry_decision
