#include <cstdint>
#include <cstdio>

#include "sentry_decision_core/referee_protocol.hpp"

using namespace sentry_decision;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void test_decode_game_status() {
  CHECK(decode_game_status(0) == GameStatus::kNotStarted);
  CHECK(decode_game_status(1) == GameStatus::kPreparation);
  CHECK(decode_game_status(2) == GameStatus::kSelfCheck);
  CHECK(decode_game_status(3) == GameStatus::kCountdown);
  CHECK(decode_game_status(4) == GameStatus::kRunning);
  CHECK(decode_game_status(5) == GameStatus::kSettling);
  CHECK(decode_game_status(6) == GameStatus::kUnknown);
  CHECK(decode_game_status(0xFF) == GameStatus::kUnknown);
}

void test_decode_event_code() {
  const std::uint32_t raw = 1u | (1u << 2) | (1u << 3) | (2u << 5) | (2u << 7) | (3u << 9) |
                            (300u << 11) | (4u << 20) | (2u << 23) | (3u << 25) | (2u << 27) |
                            (1u << 29);
  const EventCode event = decode_event_code(raw);
  CHECK(event.supply_zone_occupied);
  CHECK(event.supply_zone_occupied_rmul);
  CHECK(event.small_energy_status == 1);
  CHECK(event.big_energy_status == 2);
  CHECK(event.central_highland_status == 2);
  CHECK(event.trapezoid_highland_status == 3);
  CHECK(event.enemy_dart_last_hit_time_s == 300);
  CHECK(event.enemy_dart_last_target == 4);
  CHECK(event.center_buff_status == 2);
  CHECK(event.fort_occupation_status == 3);
  CHECK(event.our_outpost_buff_status == 2);
  CHECK(event.base_buff_occupied);

  // 全 1：各字段应被掩码截断到本段宽度。
  const EventCode all = decode_event_code(0xFFFFFFFFu);
  CHECK(all.supply_zone_occupied);
  CHECK(all.small_energy_status == 3);
  CHECK(all.big_energy_status == 3);
  CHECK(all.central_highland_status == 3);
  CHECK(all.enemy_dart_last_hit_time_s == 0x1FF);
  CHECK(all.enemy_dart_last_target == 7);
  CHECK(all.fort_occupation_status == 3);
  CHECK(all.base_buff_occupied);
}

void test_decode_sentry_info1() {
  const std::uint32_t raw = 291u | (5u << 11) | (9u << 15) | (1u << 19) | (1u << 20) | (300u << 21);
  const SentryInfo1 info = decode_sentry_info1(raw);
  CHECK(info.ammo_exchanged_local == 291);
  CHECK(info.remote_ammo_exchange_count == 5);
  CHECK(info.remote_health_exchange_count == 9);
  CHECK(info.can_free_resurrect);
  CHECK(info.can_instant_resurrect);
  CHECK(info.instant_resurrect_cost == 300);

  const SentryInfo1 all = decode_sentry_info1(0xFFFFFFFFu);
  CHECK(all.ammo_exchanged_local == 0x7FF);
  CHECK(all.remote_ammo_exchange_count == 0xF);
  CHECK(all.remote_health_exchange_count == 0xF);
  CHECK(all.can_free_resurrect);
  CHECK(all.can_instant_resurrect);
  CHECK(all.instant_resurrect_cost == 0x3FF);
}

void test_decode_sentry_info2() {
  const std::uint16_t raw =
      static_cast<std::uint16_t>(1u | (0x7FFu << 1) | (2u << 12) | (1u << 14) | (1u << 15));
  const SentryInfo2 info = decode_sentry_info2(raw);
  CHECK(info.disengaged);
  CHECK(info.remaining_ammo_exchange == 0x7FF);
  CHECK(info.stance == SentryStance::kDefense);
  CHECK(info.can_activate_energy);
  CHECK(info.stance_enhanced);

  const SentryInfo2 none = decode_sentry_info2(0);
  CHECK(!none.disengaged);
  CHECK(none.remaining_ammo_exchange == 0);
  CHECK(none.stance == SentryStance::kUnknown);
  CHECK(!none.can_activate_energy);
  CHECK(!none.stance_enhanced);
}

void test_decode_stance() {
  CHECK(decode_stance(1) == SentryStance::kAttack);
  CHECK(decode_stance(2) == SentryStance::kDefense);
  CHECK(decode_stance(3) == SentryStance::kMove);
  CHECK(decode_stance(0) == SentryStance::kUnknown);
  CHECK(decode_stance(9) == SentryStance::kUnknown);
}

void test_decode_sentry_info3() {
  const std::uint64_t raw =
      10ull | (20ull << 8) | (30ull << 16) | (40ull << 32) | (50ull << 40) | (60ull << 48);
  const SentryInfo3 info = decode_sentry_info3(raw);
  CHECK(info.attack_stance_remaining_s == 10);
  CHECK(info.defense_stance_remaining_s == 20);
  CHECK(info.move_stance_remaining_s == 30);
  CHECK(info.attack_enhanced_remaining_s == 40);
  CHECK(info.defense_enhanced_remaining_s == 50);
  CHECK(info.move_enhanced_remaining_s == 60);

  const SentryInfo3 none = decode_sentry_info3(0);
  CHECK(none.move_stance_remaining_s == 0);
  CHECK(none.attack_enhanced_remaining_s == 0);

  const SentryInfo3 all = decode_sentry_info3(~0ull);
  CHECK(all.attack_stance_remaining_s == 0xFF);
  CHECK(all.move_enhanced_remaining_s == 0xFF);
}

}  // namespace

int main() {
  test_decode_game_status();
  test_decode_event_code();
  test_decode_sentry_info1();
  test_decode_sentry_info2();
  test_decode_stance();
  test_decode_sentry_info3();
  if (g_failures == 0) {
    std::printf("all core referee_protocol tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
