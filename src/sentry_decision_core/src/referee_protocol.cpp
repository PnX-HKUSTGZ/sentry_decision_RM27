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

EventCode decode_event_code(std::uint32_t raw) {
  EventCode out;
  out.supply_zone_occupied = bits(raw, 0, 1) != 0;
  out.supply_zone_occupied_rmul = bits(raw, 2, 1) != 0;
  out.small_energy_status = static_cast<std::uint8_t>(bits(raw, 3, 2));
  out.big_energy_status = static_cast<std::uint8_t>(bits(raw, 5, 2));
  out.central_highland_status = static_cast<std::uint8_t>(bits(raw, 7, 2));
  out.trapezoid_highland_status = static_cast<std::uint8_t>(bits(raw, 9, 2));
  out.enemy_dart_last_hit_time_s = static_cast<std::uint16_t>(bits(raw, 11, 9));
  out.enemy_dart_last_target = static_cast<std::uint8_t>(bits(raw, 20, 3));
  out.center_buff_status = static_cast<std::uint8_t>(bits(raw, 23, 2));
  out.fort_occupation_status = static_cast<std::uint8_t>(bits(raw, 25, 2));
  out.our_outpost_buff_status = static_cast<std::uint8_t>(bits(raw, 27, 2));
  out.base_buff_occupied = bits(raw, 29, 1) != 0;
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
