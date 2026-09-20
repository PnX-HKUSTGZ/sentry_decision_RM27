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

EventCode decode_event_code(std::uint32_t raw) {
  EventCode out;
  out.small_energy_status = static_cast<std::uint8_t>(bits(raw, 3, 2));
  out.big_energy_status = static_cast<std::uint8_t>(bits(raw, 5, 2));
  out.fort_occupation_status = static_cast<std::uint8_t>(bits(raw, 25, 2));
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
  out.can_activate_energy = bits(raw, 14, 1) != 0;
  return out;
}

}  // namespace sentry_decision
