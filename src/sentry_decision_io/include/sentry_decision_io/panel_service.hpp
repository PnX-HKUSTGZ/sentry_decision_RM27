#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_msgs/srv/debug_command.hpp"

namespace sentry_decision_io {

// 面板 service 入队的战术层覆盖命令。
struct TacticalOverrideCommand {
  enum class Kind { kSet, kClear };
  Kind kind = Kind::kSet;
  sentry_decision::TacticalMode mode = sentry_decision::TacticalMode::kUnknown;
  // lease 为 0 表示不过期。
  sentry_decision::Duration lease{0};
};

// 解析面板 service 的 command + args（args 为 YAML/JSON 文本）。
// list_state 不产命令，只置 *list_state = true；失败写 *error。
bool parse_panel_command(const std::string& command, const std::string& args,
                         std::vector<TacticalOverrideCommand>* out, bool* list_state,
                         std::string* error);

// 面板 service：list_state（只读）/ set_tactical_mode / clear_tactical_mode。
//
// 线程模型：service 回调只校验并入队，不触碰行为树；决策 tick 线程在 tick 边界
// 用 take_commands() 取走并应用到 TacticalOverride。
class PanelService {
 public:
  using DebugCommand = sentry_decision_msgs::srv::DebugCommand;
  // 返回 list_state 的 JSON 快照；由组合根提供，需自行保证线程安全。
  using StateProvider = std::function<std::string()>;

  PanelService(rclcpp::Node& node, const std::string& service_name, StateProvider state_provider);

  std::vector<TacticalOverrideCommand> take_commands();

 private:
  void handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                    std::shared_ptr<DebugCommand::Response> response);

  std::mutex mutex_;
  std::vector<TacticalOverrideCommand> commands_;
  StateProvider state_provider_;
  rclcpp::Service<DebugCommand>::SharedPtr service_;
};

}  // namespace sentry_decision_io