#include "sentry_decision_io/intervention_convert.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include "sentry_decision_core/intervention.hpp"

namespace sentry_decision_io {
namespace {

using sentry_decision::Duration;
using sentry_decision::Intent;
using sentry_decision::IntentField;
using sentry_decision::Point2D;
using sentry_decision::ResourceRequest;
using sentry_decision::TacticalMode;
using sentry_decision::Twist;
using sentry_decision::WorldField;

bool field_from_name(const std::string& name, IntentField* out) {
  if (name == "nav_goal") {
    *out = IntentField::kNavGoal;
  } else if (name == "chassis_vel") {
    *out = IntentField::kChassisVel;
  } else if (name == "resource_request") {
    *out = IntentField::kResourceRequest;
  } else if (name == "tactical_mode") {
    *out = IntentField::kTacticalMode;
  } else {
    return false;
  }
  return true;
}

bool parse_point(const YAML::Node& node, Point2D* out) {
  if (!node.IsSequence() || node.size() < 2 || node.size() > 3) {
    return false;
  }
  try {
    out->x = node[0].as<double>();
    out->y = node[1].as<double>();
    out->yaw = node.size() == 3 ? node[2].as<double>() : 0.0;
  } catch (const std::exception&) {
    return false;
  }
  return true;
}

bool parse_twist(const YAML::Node& node, Twist* out) {
  if (!node.IsSequence() || node.size() != 3) {
    return false;
  }
  try {
    out->vx = node[0].as<double>();
    out->vy = node[1].as<double>();
    out->wz = node[2].as<double>();
  } catch (const std::exception&) {
    return false;
  }
  return true;
}

bool parse_resource(const YAML::Node& node, ResourceRequest* out) {
  if (!node.IsMap()) {
    return false;
  }
  try {
    if (node["ammo"]) {
      out->ammo = node["ammo"].as<int>();
    }
    if (node["hp"]) {
      out->hp = node["hp"].as<int>();
    }
    if (node["revive"]) {
      out->revive = node["revive"].as<bool>();
    }
  } catch (const std::exception&) {
    return false;
  }
  return true;
}

bool parse_tactical_mode(const YAML::Node& node, TacticalMode* out) {
  if (!node.IsScalar()) {
    return false;
  }
  const std::string text = node.Scalar();
  if (text == "unknown") {
    *out = TacticalMode::kUnknown;
    return true;
  }
  if (text == "patrol") {
    *out = TacticalMode::kPatrol;
    return true;
  }
  if (text == "attack") {
    *out = TacticalMode::kAttack;
    return true;
  }
  if (text == "defend") {
    *out = TacticalMode::kDefend;
    return true;
  }
  if (text == "retreat") {
    *out = TacticalMode::kRetreat;
    return true;
  }
  if (text == "heal") {
    *out = TacticalMode::kHeal;
    return true;
  }
  if (text == "respawn") {
    *out = TacticalMode::kRespawn;
    return true;
  }
  try {
    const int value = node.as<int>();
    if (value < 0 || value > 6) {
      return false;
    }
    *out = static_cast<TacticalMode>(value);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool parse_value_node(IntentField field, const YAML::Node& node, Intent* out, std::string* error) {
  switch (field) {
    case IntentField::kNavGoal: {
      Point2D point;
      if (!parse_point(node, &point)) {
        *error = "nav_goal 需要 [x, y] 或 [x, y, yaw]";
        return false;
      }
      out->value = point;
      return true;
    }
    case IntentField::kChassisVel: {
      Twist twist;
      if (!parse_twist(node, &twist)) {
        *error = "chassis_vel 需要 [vx, vy, wz]";
        return false;
      }
      out->value = twist;
      return true;
    }
    case IntentField::kResourceRequest: {
      ResourceRequest request;
      if (!parse_resource(node, &request)) {
        *error = "resource_request 需要 {ammo, hp, revive}";
        return false;
      }
      out->value = request;
      return true;
    }
    case IntentField::kTacticalMode: {
      TacticalMode mode;
      if (!parse_tactical_mode(node, &mode)) {
        *error = "tactical_mode 需要名称（patrol 等）或 0-6";
        return false;
      }
      out->value = mode;
      return true;
    }
  }
  *error = "未知意图字段";
  return false;
}

Duration lease_from_seconds(double lease_sec) {
  return Duration{static_cast<std::int64_t>(lease_sec * 1000.0 + 0.5)};
}

}  // namespace

bool parse_intent_field(std::uint8_t value, IntentField* out) {
  switch (value) {
    case 0:
      *out = IntentField::kNavGoal;
      return true;
    case 1:
      *out = IntentField::kChassisVel;
      return true;
    case 2:
      *out = IntentField::kResourceRequest;
      return true;
    case 3:
      *out = IntentField::kTacticalMode;
      return true;
    default:
      return false;
  }
}

bool parse_world_field(const std::string& name, WorldField* out) {
  if (name == "self_hp") {
    *out = WorldField::kSelfHp;
  } else if (name == "self_ammo") {
    *out = WorldField::kSelfAmmo;
  } else if (name == "our_outpost_hp") {
    *out = WorldField::kOurOutpostHp;
  } else if (name == "enemy_outpost_hp") {
    *out = WorldField::kEnemyOutpostHp;
  } else if (name == "game_time_remaining") {
    *out = WorldField::kGameTimeRemaining;
  } else if (name == "coins") {
    *out = WorldField::kCoins;
  } else {
    return false;
  }
  return true;
}

const char* intent_field_name(IntentField field) {
  switch (field) {
    case IntentField::kNavGoal:
      return "nav_goal";
    case IntentField::kChassisVel:
      return "chassis_vel";
    case IntentField::kResourceRequest:
      return "resource_request";
    case IntentField::kTacticalMode:
      return "tactical_mode";
  }
  return "unknown";
}

const char* world_field_name(WorldField field) {
  switch (field) {
    case WorldField::kSelfHp:
      return "self_hp";
    case WorldField::kSelfAmmo:
      return "self_ammo";
    case WorldField::kOurOutpostHp:
      return "our_outpost_hp";
    case WorldField::kEnemyOutpostHp:
      return "enemy_outpost_hp";
    case WorldField::kGameTimeRemaining:
      return "game_time_remaining";
    case WorldField::kCoins:
      return "coins";
  }
  return "unknown";
}

const char* source_name(sentry_decision::SourceId source) {
  switch (source) {
    case sentry_decision::SourceId::kStrategic:
      return "strategic";
    case sentry_decision::SourceId::kMission:
      return "mission";
    case sentry_decision::SourceId::kSkill:
      return "skill";
    case sentry_decision::SourceId::kRecovery:
      return "recovery";
    case sentry_decision::SourceId::kIntervention:
      return "intervention";
    case sentry_decision::SourceId::kSupervisor:
      return "supervisor";
  }
  return "unknown";
}

bool parse_manual_override(std::uint8_t field, const std::string& value, double lease_sec,
                           const std::string& reason, InterventionCommand* out,
                           std::string* error) {
  IntentField core_field{};
  if (!parse_intent_field(field, &core_field)) {
    *error = "未知 field 常量: " + std::to_string(field);
    return false;
  }
  if (!(lease_sec >= 0.0)) {
    *error = "lease_sec 必须 >= 0";
    return false;
  }
  YAML::Node node;
  try {
    node = YAML::Load(value);
  } catch (const std::exception& ex) {
    *error = std::string("value 解析失败: ") + ex.what();
    return false;
  }
  if (!node || node.IsNull()) {
    *error = "value 不能为空";
    return false;
  }

  Intent intent;
  intent.field = core_field;
  std::string detail;
  if (!parse_value_node(core_field, node, &intent, &detail)) {
    *error = detail;
    return false;
  }
  intent.lease = lease_from_seconds(lease_sec);

  out->kind = InterventionCommand::Kind::kIntent;
  out->intent = intent;
  out->value_text = value;
  out->reason = reason;
  return true;
}

bool parse_debug_command(const std::string& command, const std::string& args,
                         std::vector<InterventionCommand>* out, bool* list_state,
                         std::string* error) {
  *list_state = false;
  YAML::Node node;
  if (!args.empty()) {
    try {
      node = YAML::Load(args);
    } catch (const std::exception& ex) {
      *error = std::string("args 解析失败: ") + ex.what();
      return false;
    }
  }

  if (command == "list_state") {
    *list_state = true;
    return true;
  }
  if (command == "clear_all") {
    InterventionCommand cmd;
    cmd.kind = InterventionCommand::Kind::kClearAll;
    out->push_back(cmd);
    return true;
  }
  if (command == "set_intent") {
    if (!node.IsMap() || !node["field"] || !node["value"]) {
      *error = "set_intent 需要 field 与 value";
      return false;
    }
    IntentField field{};
    if (!field_from_name(node["field"].as<std::string>(), &field)) {
      *error = "未知意图字段: " + node["field"].as<std::string>();
      return false;
    }
    InterventionCommand cmd;
    cmd.kind = InterventionCommand::Kind::kIntent;
    cmd.intent.field = field;
    std::string detail;
    if (!parse_value_node(field, node["value"], &cmd.intent, &detail)) {
      *error = "set_intent: " + detail;
      return false;
    }
    const double lease_sec = node["lease_sec"] ? node["lease_sec"].as<double>() : 0.0;
    if (!(lease_sec >= 0.0)) {
      *error = "set_intent: lease_sec 必须 >= 0";
      return false;
    }
    cmd.intent.lease = lease_from_seconds(lease_sec);
    cmd.value_text = YAML::Dump(node["value"]);
    cmd.reason = node["reason"] ? node["reason"].as<std::string>() : "debug";
    out->push_back(cmd);
    return true;
  }
  if (command == "clear_intent") {
    InterventionCommand cmd;
    cmd.kind = InterventionCommand::Kind::kClearIntent;
    if (node.IsMap() && node["field"]) {
      if (!field_from_name(node["field"].as<std::string>(), &cmd.intent_field)) {
        *error = "clear_intent: 未知字段";
        return false;
      }
    }
    out->push_back(cmd);
    return true;
  }
  if (command == "set_world") {
    if (!node.IsMap() || !node["field"] || !node["value"]) {
      *error = "set_world 需要 field 与 value";
      return false;
    }
    InterventionCommand cmd;
    cmd.kind = InterventionCommand::Kind::kWorldOverride;
    if (!parse_world_field(node["field"].as<std::string>(), &cmd.world_field)) {
      *error = "set_world: 未知字段 " + node["field"].as<std::string>();
      return false;
    }
    cmd.world_value = node["value"].as<double>();
    out->push_back(cmd);
    return true;
  }
  if (command == "clear_world") {
    if (node.IsMap() && node["field"]) {
      InterventionCommand cmd;
      cmd.kind = InterventionCommand::Kind::kClearWorld;
      if (!parse_world_field(node["field"].as<std::string>(), &cmd.world_field)) {
        *error = "clear_world: 未知字段";
        return false;
      }
      out->push_back(cmd);
      return true;
    }
    const WorldField all[] = {
        WorldField::kSelfHp,         WorldField::kSelfAmmo,          WorldField::kOurOutpostHp,
        WorldField::kEnemyOutpostHp, WorldField::kGameTimeRemaining, WorldField::kCoins};
    for (WorldField field : all) {
      InterventionCommand cmd;
      cmd.kind = InterventionCommand::Kind::kClearWorld;
      cmd.world_field = field;
      out->push_back(cmd);
    }
    return true;
  }
  if (command == "set_module") {
    if (!node.IsMap() || !node["module"]) {
      *error = "set_module 需要 module";
      return false;
    }
    InterventionCommand cmd;
    cmd.kind = InterventionCommand::Kind::kModuleSwitch;
    cmd.module = node["module"].as<std::string>();
    cmd.enabled = node["enabled"] ? node["enabled"].as<bool>() : true;
    out->push_back(cmd);
    return true;
  }

  *error = "未知 command: " + command;
  return false;
}

}  // namespace sentry_decision_io
