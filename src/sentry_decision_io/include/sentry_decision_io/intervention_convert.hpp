#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_io/intervention_types.hpp"

namespace sentry_decision_io {

// field 常量与 sentry_decision_msgs/ManualOverride.action 一致。
bool parse_intent_field(std::uint8_t value, sentry_decision::IntentField* out);
bool parse_world_field(const std::string& name, sentry_decision::WorldField* out);

const char* intent_field_name(sentry_decision::IntentField field);
const char* world_field_name(sentry_decision::WorldField field);
const char* source_name(sentry_decision::SourceId source);

// 把 action goal 的 (field, value 文本, lease_sec) 解析成一条 kIntent 命令。
// value 用 YAML/JSON 文本表示，按 field 解析；解析失败返回 false 并写 error。
bool parse_manual_override(std::uint8_t field, const std::string& value, double lease_sec,
                           const std::string& reason, InterventionCommand* out, std::string* error);

// 解析 DebugCommand 的 command + args，产出一到多条命令。
// list_state 命令不产命令，只置 *list_state = true。
bool parse_debug_command(const std::string& command, const std::string& args,
                         std::vector<InterventionCommand>* out, bool* list_state,
                         std::string* error);

}  // namespace sentry_decision_io
