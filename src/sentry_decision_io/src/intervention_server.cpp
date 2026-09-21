#include "sentry_decision_io/intervention_server.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_io/intervention_convert.hpp"

namespace sentry_decision_io {
namespace {

bool is_active(const std::vector<sentry_decision::IntentField>& active_fields,
               sentry_decision::IntentField field) {
  return std::find(active_fields.begin(), active_fields.end(), field) != active_fields.end();
}

}  // namespace

InterventionServer::InterventionServer(rclcpp::Node& node, const std::string& action_name,
                                       const std::string& service_name,
                                       StateProvider state_provider)
    : state_provider_(std::move(state_provider)) {
  using namespace std::placeholders;
  action_server_ = rclcpp_action::create_server<ManualOverride>(
      &node, action_name, std::bind(&InterventionServer::handle_goal, this, _1, _2),
      std::bind(&InterventionServer::handle_cancel, this, _1),
      std::bind(&InterventionServer::handle_accepted, this, _1));
  service_ = node.create_service<DebugCommand>(
      service_name, std::bind(&InterventionServer::handle_debug, this, _1, _2));
}

std::vector<InterventionCommand> InterventionServer::take_commands() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<InterventionCommand> commands;
  commands.swap(commands_);
  return commands;
}

rclcpp_action::GoalResponse InterventionServer::handle_goal(
    const rclcpp_action::GoalUUID&, std::shared_ptr<const ManualOverride::Goal> goal) {
  InterventionCommand command;
  std::string error;
  if (!parse_manual_override(goal->field, goal->value, goal->lease_sec, goal->reason, &command,
                             &error)) {
    SD_LOG_WARN("intervention", "拒绝 ManualOverride: %s", error.c_str());
    return rclcpp_action::GoalResponse::REJECT;
  }
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse InterventionServer::handle_cancel(
    const std::shared_ptr<GoalHandle>& handle) {
  InterventionCommand command;
  command.kind = InterventionCommand::Kind::kClearIntent;
  const auto goal = handle->get_goal();
  if (!parse_intent_field(goal->field, &command.intent_field)) {
    return rclcpp_action::CancelResponse::REJECT;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  commands_.push_back(command);
  goals_.erase(std::remove_if(goals_.begin(), goals_.end(),
                              [&handle](const ActiveGoal& item) { return item.handle == handle; }),
               goals_.end());
  return rclcpp_action::CancelResponse::ACCEPT;
}

void InterventionServer::handle_accepted(const std::shared_ptr<GoalHandle>& handle) {
  InterventionCommand command;
  std::string error;
  const auto goal = handle->get_goal();
  if (!parse_manual_override(goal->field, goal->value, goal->lease_sec, goal->reason, &command,
                             &error)) {
    auto result = std::make_shared<ManualOverride::Result>();
    result->accepted = false;
    result->message = error;
    handle->abort(result);
    return;
  }
  sentry_decision::IntentField field{};
  parse_intent_field(goal->field, &field);

  std::lock_guard<std::mutex> lock(mutex_);
  commands_.push_back(command);
  // goal 在 override 存活期间保持执行态：update_status 每 tick 反馈胜负，
  // 待其失效 / 被清除时再 succeed。
  goals_.push_back(ActiveGoal{field, handle});
}

void InterventionServer::handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                                      std::shared_ptr<DebugCommand::Response> response) {
  std::vector<InterventionCommand> commands;
  bool list_state = false;
  std::string error;
  if (!parse_debug_command(request->command, request->args, &commands, &list_state, &error)) {
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

void InterventionServer::update_status(
    const std::map<sentry_decision::IntentField, sentry_decision::SourceId>& winners,
    const std::vector<sentry_decision::IntentField>& active_fields) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = goals_.begin(); it != goals_.end();) {
    if (!is_active(active_fields, it->field)) {
      auto result = std::make_shared<ManualOverride::Result>();
      result->accepted = true;
      result->message = "override finished";
      it->handle->succeed(result);
      it = goals_.erase(it);
      continue;
    }
    const auto winner = winners.find(it->field);
    auto feedback = std::make_shared<ManualOverride::Feedback>();
    feedback->effective =
        winner != winners.end() && winner->second == sentry_decision::SourceId::kIntervention;
    feedback->overridden_by =
        (!feedback->effective && winner != winners.end()) ? source_name(winner->second) : "";
    it->handle->publish_feedback(feedback);
    ++it;
  }
}

}  // namespace sentry_decision_io
