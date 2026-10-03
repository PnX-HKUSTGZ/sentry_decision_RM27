#include "sentry_decision_io/panel_service.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <exception>
#include <iterator>
#include <string>
#include <utility>

namespace sentry_decision_io {
namespace {

using sentry_decision::Duration;
using sentry_decision::TacticalMode;

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
  if (text == "idle") {
    *out = TacticalMode::kIdle;
    return true;
  }
  try {
    const int value = node.as<int>();
    if (value < 0 || value > 7) {
      return false;
    }
    *out = static_cast<TacticalMode>(value);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

Duration lease_from_seconds(double lease_sec) {
  return Duration{static_cast<std::int64_t>(lease_sec * 1000.0 + 0.5)};
}

}  // namespace

bool parse_panel_command(const std::string& command, const std::string& args,
                         std::vector<TacticalOverrideCommand>* out, bool* list_state,
                         std::string* error) {
  *list_state = false;
  if (command == "list_state") {
    *list_state = true;
    return true;
  }
  YAML::Node node;
  if (!args.empty()) {
    try {
      node = YAML::Load(args);
    } catch (const std::exception& ex) {
      *error = std::string("args 解析失败: ") + ex.what();
      return false;
    }
  }
  if (command == "clear_tactical_mode") {
    TacticalOverrideCommand cmd;
    cmd.kind = TacticalOverrideCommand::Kind::kClear;
    out->push_back(cmd);
    return true;
  }
  if (command == "set_tactical_mode") {
    if (!node.IsMap() || !node["mode"]) {
      *error = "set_tactical_mode 需要 mode";
      return false;
    }
    TacticalOverrideCommand cmd;
    cmd.kind = TacticalOverrideCommand::Kind::kSet;
    if (!parse_tactical_mode(node["mode"], &cmd.mode)) {
      *error = "mode 需要名称（patrol / idle 等）或 0-7";
      return false;
    }
    const double lease_sec = node["lease_sec"] ? node["lease_sec"].as<double>() : 0.0;
    if (!(lease_sec >= 0.0)) {
      *error = "lease_sec 必须 >= 0";
      return false;
    }
    cmd.lease = lease_from_seconds(lease_sec);
    out->push_back(cmd);
    return true;
  }
  *error = "未知 command: " + command;
  return false;
}

PanelService::PanelService(rclcpp::Node& node, const std::string& service_name,
                           StateProvider state_provider)
    : state_provider_(std::move(state_provider)) {
  service_ = node.create_service<DebugCommand>(
      service_name, [this](const std::shared_ptr<DebugCommand::Request> request,
                           std::shared_ptr<DebugCommand::Response> response) {
        handle_debug(request, response);
      });
}

std::vector<TacticalOverrideCommand> PanelService::take_commands() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TacticalOverrideCommand> commands;
  commands.swap(commands_);
  return commands;
}

void PanelService::handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                                std::shared_ptr<DebugCommand::Response> response) {
  std::vector<TacticalOverrideCommand> commands;
  bool list_state = false;
  std::string error;
  if (!parse_panel_command(request->command, request->args, &commands, &list_state, &error)) {
    response->success = false;
    response->message = error;
    return;
  }
  if (list_state) {
    response->success = true;
    response->message = "ok";
    response->state_json = state_provider_ ? state_provider_() : std::string();
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.insert(commands_.end(), std::make_move_iterator(commands.begin()),
                     std::make_move_iterator(commands.end()));
  }
  response->success = true;
  response->message = "queued";
}

}  // namespace sentry_decision_io