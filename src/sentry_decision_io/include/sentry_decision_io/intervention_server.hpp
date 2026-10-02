#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_io/intervention_types.hpp"
#include "sentry_decision_msgs/action/manual_override.hpp"
#include "sentry_decision_msgs/srv/debug_command.hpp"

namespace sentry_decision_io {

// 人工干预 / 调试的 ROS 服务端。
//
// 线程模型：action / service 回调只校验并入队，不触碰 WorldState 与行为树；
// 决策 tick 线程在 tick 边界用 take_commands() 取走并应用到 InterventionController。
// 这样即使决策节点跑在 MultiThreadedExecutor，也不需要跨线程共享决策状态。
class InterventionServer {
 public:
  using ManualOverride = sentry_decision_msgs::action::ManualOverride;
  using GoalHandle = rclcpp_action::ServerGoalHandle<ManualOverride>;
  using DebugCommand = sentry_decision_msgs::srv::DebugCommand;
  // 返回 list_state 的 JSON 快照；由组合根提供，需自行保证线程安全。
  using StateProvider = std::function<std::string()>;

  InterventionServer(rclcpp::Node& node, const std::string& action_name,
                     const std::string& service_name, StateProvider state_provider);

  // 取走自上次调用以来入队的所有命令。
  std::vector<InterventionCommand> take_commands();

  // 决策线程每 tick 调用：向活跃 goal 反馈逐字段胜负，并在 override 失效时结束它。
  void update_status(
      const std::map<sentry_decision::IntentField, sentry_decision::SourceId>& winners,
      const std::vector<sentry_decision::IntentField>& active_fields);

 private:
  rclcpp_action::GoalResponse handle_goal(const rclcpp_action::GoalUUID& uuid,
                                          std::shared_ptr<const ManualOverride::Goal> goal);
  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandle>& handle);
  void handle_accepted(const std::shared_ptr<GoalHandle>& handle);
  void handle_debug(const std::shared_ptr<DebugCommand::Request> request,
                    std::shared_ptr<DebugCommand::Response> response);

  struct ActiveGoal {
    sentry_decision::IntentField field;
    std::shared_ptr<GoalHandle> handle;
  };

  std::mutex mutex_;
  std::vector<InterventionCommand> commands_;
  std::vector<ActiveGoal> goals_;
  StateProvider state_provider_;
  rclcpp_action::Server<ManualOverride>::SharedPtr action_server_;
  rclcpp::Service<DebugCommand>::SharedPtr service_;
};

}  // namespace sentry_decision_io
