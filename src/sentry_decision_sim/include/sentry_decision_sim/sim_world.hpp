#pragma once

#include <cstdint>
#include <string>

#include "sentry_decision_sim/scenario.hpp"

namespace sentry_decision_sim {

// 消息级仿真世界：字段与 sentry_interfaces 上行消息一一对应。
//
// 为什么不复用 core 的 RefereeState：那边存的是解码后的 event / info1 / info2 / info3，
// 反向编码回原始位段会丢失信息、也更容易写错；仿真只需要「发出正确的消息」，
// 因此这里保存原始消息字段。
struct SimWorld {
  // GameInfo
  int game_status = 0;
  int game_time_remaining = 0;
  int coins = 0;
  unsigned int event_code = 0;
  int detect_color = 0;
  bool can_rebuild_outpost = false;
  double manual_point_x = 0.0;
  double manual_point_y = 0.0;
  int manual_key = 0;
  int enemy_outpost_hp = 0;
  int enemy_base_hp = 0;

  // SentryInfoOnline
  int self_hp = 0;
  int self_ammo = 0;
  int cooling_value = 0;
  int heat_limit = 0;
  int current_heat = 0;
  int energy_ratio = 0;
  double speed_monitor_angle = 0.0;
  unsigned int sentry_info_1 = 0;
  int sentry_info_2 = 0;
  std::uint64_t sentry_info_3 = 0;

  // TeamInfo
  int base_hp = 0;
  int our_outpost_hp = 0;

  // RadarInfo
  int enemy_coin_left = 0;
  int enemy_coin_accumulated = 0;
  bool is_enemy_outpost_sensed = false;
};

// 应用 set_world 的一个字段。未知字段返回 false 并写 error。
bool apply_world_field(SimWorld* world, const std::string& field, const ScenarioValue& value,
                       std::string* error);

// 决策输出的纯视图，供 expect 断言，避免依赖 ROS 消息。
struct DecisionView {
  int tactical_mode = 0;
  bool has_nav_goal = false;
  double nav_goal_x = 0.0;
  double nav_goal_y = 0.0;
  bool has_cmd_vel = false;
  int resource_ammo = 0;
  int resource_hp = 0;
  bool resource_revive = false;
};

// 校验一个 expect 字段：满足返回 true；不满足或字段未知返回 false 并写 error。
bool check_expect(const DecisionView& view, const std::string& field, const ScenarioValue& expected,
                  double tolerance, std::string* error);

}  // namespace sentry_decision_sim
