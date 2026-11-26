#pragma once

#include <functional>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <string>

#include "sentry_decision_msgs/srv/debug_command.hpp"

namespace sentry_decision_io {

// 解析面板 service 的 command + args；当前只支持只读 list_state。
bool parse_panel_command(const std::string& command, const std::string& args, bool* list_state,
                         std::string* error);

// 面板 service：只读 list_state，不提供决策内部写入口。
class PanelService {
 public:
  using DebugCommand = sentry_decision_msgs::srv::DebugCommand;
  // 返回 list_state 的 JSON 快照；由组合根提供，需自行保证线程安全。
  using StateProvider = std::function<std::string()>;

  PanelService(rclcpp::Node& node, const std::string& service_name, StateProvider state_provider);

 private:
  void handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                    std::shared_ptr<DebugCommand::Response> response);

  StateProvider state_provider_;
  rclcpp::Service<DebugCommand>::SharedPtr service_;
};

}  // namespace sentry_decision_io
