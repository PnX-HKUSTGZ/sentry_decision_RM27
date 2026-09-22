#include "sentry_decision_sim/sim_world.hpp"

#include <cmath>
#include <string>

namespace sentry_decision_sim {
namespace {

bool parse_stance_name(const std::string& name, int* out) {
  if (name == "unknown") {
    *out = 0;
  } else if (name == "attack") {
    *out = 1;
  } else if (name == "defense" || name == "defence") {
    *out = 2;
  } else if (name == "move") {
    *out = 3;
  } else {
    return false;
  }
  return true;
}

bool parse_tactical_mode(const std::string& name, int* out) {
  if (name == "unknown") {
    *out = 0;
  } else if (name == "patrol") {
    *out = 1;
  } else if (name == "attack") {
    *out = 2;
  } else if (name == "defend") {
    *out = 3;
  } else if (name == "retreat") {
    *out = 4;
  } else if (name == "heal") {
    *out = 5;
  } else if (name == "respawn") {
    *out = 6;
  } else {
    return false;
  }
  return true;
}

}  // namespace

bool apply_world_field(SimWorld* world, const std::string& field, const ScenarioValue& value,
                       std::string* error) {
  if (value.type == ScenarioValue::Type::kString) {
    *error = "set_world 不支持字符串取值: " + field;
    return false;
  }
  const double number =
      value.type == ScenarioValue::Type::kBool ? (value.boolean ? 1.0 : 0.0) : value.number;
  const int as_int = static_cast<int>(number);

  if (field == "game_status") {
    world->game_status = as_int;
  } else if (field == "game_time_remaining") {
    world->game_time_remaining = as_int;
  } else if (field == "coins") {
    world->coins = as_int;
  } else if (field == "event_code") {
    world->event_code = static_cast<unsigned int>(as_int);
  } else if (field == "detect_color") {
    world->detect_color = as_int;
  } else if (field == "can_rebuild_outpost") {
    world->can_rebuild_outpost = number != 0.0;
  } else if (field == "manual_point_x") {
    world->manual_point_x = number;
  } else if (field == "manual_point_y") {
    world->manual_point_y = number;
  } else if (field == "manual_key") {
    world->manual_key = as_int;
  } else if (field == "enemy_outpost_hp") {
    world->enemy_outpost_hp = as_int;
  } else if (field == "enemy_base_hp") {
    world->enemy_base_hp = as_int;
  } else if (field == "self_hp") {
    world->self_hp = as_int;
  } else if (field == "self_ammo") {
    world->self_ammo = as_int;
  } else if (field == "cooling_value") {
    world->cooling_value = as_int;
  } else if (field == "heat_limit") {
    world->heat_limit = as_int;
  } else if (field == "current_heat") {
    world->current_heat = as_int;
  } else if (field == "energy_ratio") {
    world->energy_ratio = as_int;
  } else if (field == "speed_monitor_angle") {
    world->speed_monitor_angle = number;
  } else if (field == "sentry_info_1") {
    world->sentry_info_1 = static_cast<unsigned int>(as_int);
  } else if (field == "sentry_info_2") {
    world->sentry_info_2 = as_int;
  } else if (field == "sentry_info_3") {
    world->sentry_info_3 = static_cast<std::uint64_t>(number);
  } else if (field == "stance") {
    // 语义字段：写入 sentry_info_2 的 bit 12-13。
    const int value = as_int & 0x3;
    world->sentry_info_2 = (world->sentry_info_2 & ~(0x3 << 12)) | (value << 12);
  } else if (field == "stance_enhanced") {
    if (number != 0.0) {
      world->sentry_info_2 |= (1 << 15);
    } else {
      world->sentry_info_2 &= ~(1 << 15);
    }
  } else if (field == "base_hp") {
    world->base_hp = as_int;
  } else if (field == "our_outpost_hp") {
    world->our_outpost_hp = as_int;
  } else if (field == "enemy_coin_left") {
    world->enemy_coin_left = as_int;
  } else if (field == "enemy_coin_accumulated") {
    world->enemy_coin_accumulated = as_int;
  } else if (field == "is_enemy_outpost_sensed") {
    world->is_enemy_outpost_sensed = number != 0.0;
  } else {
    *error = "未知 set_world 字段: " + field;
    return false;
  }
  return true;
}

bool check_expect(const DecisionView& view, const std::string& field, const ScenarioValue& expected,
                  double tolerance, std::string* error) {
  const double expected_number = expected.type == ScenarioValue::Type::kBool
                                     ? (expected.boolean ? 1.0 : 0.0)
                                     : expected.number;
  const bool expected_bool =
      expected.type == ScenarioValue::Type::kBool ? expected.boolean : (expected.number != 0.0);

  if (field == "tactical_mode") {
    int want = 0;
    if (expected.type == ScenarioValue::Type::kString) {
      if (!parse_tactical_mode(expected.text, &want)) {
        *error = "未知战术模式: " + expected.text;
        return false;
      }
    } else {
      want = static_cast<int>(expected_number);
    }
    if (view.tactical_mode != want) {
      *error = "期望 tactical_mode=" + std::to_string(want) +
               "，实际=" + std::to_string(view.tactical_mode);
      return false;
    }
    return true;
  }
  if (field == "stance") {
    int want = 0;
    if (expected.type == ScenarioValue::Type::kString) {
      if (!parse_stance_name(expected.text, &want)) {
        *error = "未知姿态: " + expected.text;
        return false;
      }
    } else {
      want = static_cast<int>(expected_number);
    }
    if (view.stance != want) {
      *error = "期望 stance=" + std::to_string(want) + "，实际=" + std::to_string(view.stance);
      return false;
    }
    return true;
  }
  if (field == "has_nav_goal") {
    if (view.has_nav_goal != expected_bool) {
      *error = std::string("期望 has_nav_goal=") + (expected_bool ? "true" : "false") +
               "，实际=" + (view.has_nav_goal ? "true" : "false");
      return false;
    }
    return true;
  }
  if (field == "nav_goal_x" || field == "nav_goal_y") {
    if (!view.has_nav_goal) {
      *error = "期望 " + field + "，但当前没有导航目标";
      return false;
    }
    const double actual = field == "nav_goal_x" ? view.nav_goal_x : view.nav_goal_y;
    if (std::fabs(actual - expected_number) > tolerance) {
      *error = "期望 " + field + "=" + std::to_string(expected_number) +
               "，实际=" + std::to_string(actual);
      return false;
    }
    return true;
  }
  if (field == "has_cmd_vel") {
    if (view.has_cmd_vel != expected_bool) {
      *error = "has_cmd_vel 不匹配";
      return false;
    }
    return true;
  }
  if (field == "resource_ammo") {
    if (view.resource_ammo != static_cast<int>(expected_number)) {
      *error = "resource_ammo 不匹配";
      return false;
    }
    return true;
  }
  if (field == "resource_hp") {
    if (view.resource_hp != static_cast<int>(expected_number)) {
      *error = "resource_hp 不匹配";
      return false;
    }
    return true;
  }
  if (field == "resource_revive") {
    if (view.resource_revive != expected_bool) {
      *error = "resource_revive 不匹配";
      return false;
    }
    return true;
  }

  *error = "未知 expect 字段: " + field;
  return false;
}

}  // namespace sentry_decision_sim
