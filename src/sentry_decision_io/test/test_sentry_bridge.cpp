#include <cstdint>
#include <cstdio>

#include "sentry_decision_io/sentry_bridge.hpp"

using namespace sentry_decision;
using namespace sentry_decision_io;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void test_apply_game_info() {
  GameInfoMsg msg;
  msg.game_status = 4;
  msg.game_time_remaining = 200;
  msg.coin_remaining = 50;
  msg.event_code = (1u << 3);  // small_energy_status = 1
  msg.detect_color = 1;
  msg.can_rebuild_outpost = true;
  msg.manual_point_x = 1.5f;
  msg.manual_point_y = -2.5f;
  msg.manual_key = 65;
  msg.enemy_outpost_hp = 1200;
  msg.enemy_base_hp = 5000;

  RefereeState state;
  merge(msg, &state);
  CHECK(state.game_status == GameStatus::kRunning);
  CHECK(state.game_time_remaining == 200);
  CHECK(state.coins == 50);
  CHECK(state.event.small_energy_status == 1);
  CHECK(state.detect_color == 1);
  CHECK(state.can_rebuild_outpost);
  CHECK(state.manual_point.x == 1.5);
  CHECK(state.manual_point.y == -2.5);
  CHECK(state.manual_key == 65);
  CHECK(state.enemy_outpost_hp == 1200);
  CHECK(state.enemy_base_hp == 5000);
}

void test_apply_online_decodes_bitfields() {
  SentryInfoOnlineMsg msg;
  msg.self_health = 400;
  msg.bullets_remaining = 100;
  msg.cooling_value = 20;
  msg.heat_limit = 300;
  msg.current_heat = 80;
  msg.energy_ratio = 60;
  msg.speed_monitor_angle = 90.0f;
  msg.sentry_info_1 = 291u | (1u << 19);  // ammo_exchanged_local=291, can_free_resurrect
  msg.sentry_info_2 = static_cast<std::uint16_t>(1u | (0x7FFu << 1) | (1u << 14));

  RefereeState state;
  merge(msg, &state);
  CHECK(state.self_hp == 400);
  CHECK(state.self_ammo == 100);
  CHECK(state.cooling_value == 20);
  CHECK(state.heat_limit == 300);
  CHECK(state.current_heat == 80);
  CHECK(state.energy_ratio == 60);
  CHECK(state.gimbal_yaw_deg == 90.0);
  CHECK(state.info1.ammo_exchanged_local == 291);
  CHECK(state.info1.can_free_resurrect);
  CHECK(state.info2.disengaged);
  CHECK(state.info2.remaining_ammo_exchange == 0x7FF);
  CHECK(state.info2.can_activate_energy);
}

void test_apply_offline_team_radar() {
  SentryInfoOfflineMsg offline;
  offline.lifter_current_pos = 2;
  offline.is_transformable = true;
  offline.transform_state = 0.5f;
  offline.capacitor_capacity = 80;
  offline.tunnel_yaw_aligned = true;
  offline.yaw_camerainit_to_gimbal = 1.25f;

  TeamInfoMsg team;
  team.base_hp = 5000;
  team.outpost_hp = 1500;

  RadarInfoMsg radar;
  radar.enemy_coin_left = 30;
  radar.enemy_coin_accumulated = 120;
  radar.is_enemy_outpost_sensed = true;

  RefereeState state;
  merge(offline, &state);
  merge(team, &state);
  merge(radar, &state);
  CHECK(state.lifter_pos == 2);
  CHECK(state.is_transformable);
  CHECK(state.transform_state == 0.5);
  CHECK(state.capacitor_capacity == 80);
  CHECK(state.tunnel_yaw_aligned);
  CHECK(state.yaw_camera_to_gimbal == 1.25);
  CHECK(state.base_hp == 5000);
  CHECK(state.our_outpost_hp == 1500);
  CHECK(state.enemy_coins == 30);
  CHECK(state.enemy_coins_accumulated == 120);
  CHECK(state.enemy_outpost_sensed);
}

void test_decision_command_mapping() {
  DecisionAction action;
  action.kind = DecisionActionKind::kInstantResurrect;
  action.mode = ActionMode::kPolled;
  action.interval = Duration{200};
  action.value = 3;
  action.request_id = 42;

  const auto msg = to_msg(action);
  CHECK(msg.request_id == 42);
  CHECK(msg.action.kind == sentry_interfaces::msg::DecisionAction::KIND_INSTANT_RESURRECT);
  CHECK(msg.action.mode == sentry_interfaces::msg::DecisionAction::MODE_POLLED);
  CHECK(msg.action.interval_ms == 200);
  CHECK(msg.action.value == 3);

  DecisionAction none;
  const auto none_msg = to_msg(none);
  CHECK(none_msg.action.kind == sentry_interfaces::msg::DecisionAction::KIND_NONE);
  CHECK(none_msg.action.mode == sentry_interfaces::msg::DecisionAction::MODE_ONE_SHOT);
}

}  // namespace

int main() {
  test_apply_game_info();
  test_apply_online_decodes_bitfields();
  test_apply_offline_team_radar();
  test_decision_command_mapping();
  if (g_failures == 0) {
    std::printf("all io sentry_bridge tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
