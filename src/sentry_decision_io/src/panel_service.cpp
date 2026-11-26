#include "sentry_decision_io/panel_service.hpp"

#include <utility>

namespace sentry_decision_io {

bool parse_panel_command(const std::string& command, const std::string& /*args*/, bool* list_state,
                         std::string* error) {
  *list_state = false;
  if (command == "list_state") {
    *list_state = true;
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

void PanelService::handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                                std::shared_ptr<DebugCommand::Response> response) {
  bool list_state = false;
  std::string error;
  if (!parse_panel_command(request->command, request->args, &list_state, &error)) {
    response->success = false;
    response->message = error;
    return;
  }
  response->success = true;
  response->message = "ok";
  response->state_json = state_provider_ ? state_provider_() : std::string();
}

}  // namespace sentry_decision_io
