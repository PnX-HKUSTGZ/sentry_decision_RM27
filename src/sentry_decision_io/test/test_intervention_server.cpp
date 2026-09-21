#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_io/intervention_convert.hpp"
#include "sentry_decision_io/intervention_server.hpp"
#include "sentry_decision_msgs/action/manual_override.hpp"
#include "sentry_decision_msgs/srv/debug_command.hpp"

using namespace std::chrono_literals;
using sentry_decision_io::InterventionCommand;
using sentry_decision_io::InterventionServer;
using ManualOverride = sentry_decision_msgs::action::ManualOverride;
using DebugCommand = sentry_decision_msgs::srv::DebugCommand;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

template <typename Predicate>
bool spin_until(rclcpp::executors::SingleThreadedExecutor& exec, Predicate predicate,
                std::chrono::nanoseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    exec.spin_some(std::chrono::milliseconds(20));
    if (predicate()) {
      return true;
    }
  }
  return predicate();
}

}  // namespace

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto server_node = std::make_shared<rclcpp::Node>("intervention_server_test");
  const std::string state = "{\"world\":{}}";
  InterventionServer server(*server_node, "/decision/manual_override", "/decision/debug",
                            [state]() { return state; });

  auto client_node = std::make_shared<rclcpp::Node>("intervention_client_test");
  auto debug_client = client_node->create_client<DebugCommand>("/decision/debug");
  auto action_client =
      rclcpp_action::create_client<ManualOverride>(client_node, "/decision/manual_override");

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(server_node);
  exec.add_node(client_node);

  CHECK(spin_until(
      exec, [&]() { return debug_client->service_is_ready(); }, std::chrono::seconds(5)));
  CHECK(spin_until(
      exec, [&]() { return action_client->action_server_is_ready(); }, std::chrono::seconds(5)));

  {
    auto request = std::make_shared<DebugCommand::Request>();
    request->command = "set_world";
    request->args = "{field: self_hp, value: 20}";
    auto future = debug_client->async_send_request(request);
    CHECK(exec.spin_until_future_complete(future, 5s) == rclcpp::FutureReturnCode::SUCCESS);
    auto response = future.get();
    CHECK(response->success);
  }
  {
    auto request = std::make_shared<DebugCommand::Request>();
    request->command = "list_state";
    auto future = debug_client->async_send_request(request);
    CHECK(exec.spin_until_future_complete(future, 5s) == rclcpp::FutureReturnCode::SUCCESS);
    CHECK(future.get()->state_json == state);
  }

  rclcpp_action::Client<ManualOverride>::GoalHandle::SharedPtr handle;
  {
    ManualOverride::Goal goal;
    goal.field = 0;
    goal.value = "[1.0, 2.0]";
    goal.lease_sec = 1.0;
    goal.reason = "test";
    auto goal_future = action_client->async_send_goal(goal);
    CHECK(exec.spin_until_future_complete(goal_future, 5s) == rclcpp::FutureReturnCode::SUCCESS);
    handle = goal_future.get();
    CHECK(handle != nullptr);
    // 让 handle_accepted 完成入队。
    for (int i = 0; i < 5; ++i) {
      exec.spin_some(20ms);
    }
  }

  const std::vector<InterventionCommand> commands = server.take_commands();
  CHECK(commands.size() == 2);
  if (commands.size() == 2) {
    CHECK(commands[0].kind == InterventionCommand::Kind::kWorldOverride);
    CHECK(commands[0].world_field == sentry_decision::WorldField::kSelfHp);
    CHECK(commands[1].kind == InterventionCommand::Kind::kIntent);
    CHECK(commands[1].intent.field == sentry_decision::IntentField::kNavGoal);
  }

  if (handle != nullptr) {
    auto result_future = action_client->async_get_result(handle);
    // 字段不再活跃 -> override 结束，goal 成功。
    server.update_status(std::map<sentry_decision::IntentField, sentry_decision::SourceId>{},
                         std::vector<sentry_decision::IntentField>{});
    CHECK(exec.spin_until_future_complete(result_future, 5s) == rclcpp::FutureReturnCode::SUCCESS);
    CHECK(result_future.get().code == rclcpp_action::ResultCode::SUCCEEDED);
  }

  rclcpp::shutdown();
  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_intervention_server passed\n");
  return 0;
}
