#include <chrono>
#include <cstdio>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <vector>

#include "sentry_decision_io/panel_service.hpp"
#include "sentry_decision_msgs/srv/debug_command.hpp"

using namespace std::chrono_literals;
using sentry_decision_io::PanelService;
using sentry_decision_io::TacticalOverrideCommand;
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

std::shared_ptr<DebugCommand::Response> call(rclcpp::executors::SingleThreadedExecutor& exec,
                                             rclcpp::Client<DebugCommand>& client,
                                             const std::string& command, const std::string& args) {
  auto request = std::make_shared<DebugCommand::Request>();
  request->command = command;
  request->args = args;
  auto future = client.async_send_request(request);
  if (exec.spin_until_future_complete(future, 5s) != rclcpp::FutureReturnCode::SUCCESS) {
    return nullptr;
  }
  return future.get();
}

}  // namespace

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto server_node = std::make_shared<rclcpp::Node>("panel_service_server_test");
  const std::string state = "{\"points\":[],\"safety_emergency\":false}";
  // 独立服务名，避免与并行的集成测试 / decision_node 的 /decision/debug 冲突。
  const std::string service_name = "/panel_service_test/debug";
  PanelService server(*server_node, service_name, [state]() { return state; });

  auto client_node = std::make_shared<rclcpp::Node>("panel_service_client_test");
  auto client = client_node->create_client<DebugCommand>(service_name);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(server_node);
  exec.add_node(client_node);

  CHECK(spin_until(exec, [&]() { return client->service_is_ready(); }, 5s));

  // list_state：只读，返回 state_json，不入队。
  {
    const auto response = call(exec, *client, "list_state", "");
    CHECK(response != nullptr);
    if (response != nullptr) {
      CHECK(response->success);
      CHECK(response->state_json == state);
    }
    CHECK(server.take_commands().empty());
  }

  // set_tactical_mode：入队一条 kSet。
  {
    const auto response =
        call(exec, *client, "set_tactical_mode", "{mode: retreat, lease_sec: 2.5}");
    CHECK(response != nullptr);
    if (response != nullptr) {
      CHECK(response->success);
    }
    const std::vector<TacticalOverrideCommand> commands = server.take_commands();
    CHECK(commands.size() == 1);
    if (commands.size() == 1) {
      CHECK(commands[0].kind == TacticalOverrideCommand::Kind::kSet);
      CHECK(commands[0].mode == sentry_decision::TacticalMode::kRetreat);
      CHECK(commands[0].lease == sentry_decision::Duration{2500});
    }
  }

  // clear_tactical_mode：入队一条 kClear。
  {
    const auto response = call(exec, *client, "clear_tactical_mode", "");
    CHECK(response != nullptr);
    if (response != nullptr) {
      CHECK(response->success);
    }
    const std::vector<TacticalOverrideCommand> commands = server.take_commands();
    CHECK(commands.size() == 1);
    if (commands.size() == 1) {
      CHECK(commands[0].kind == TacticalOverrideCommand::Kind::kClear);
    }
  }

  // 非法 lease_sec：返回 success=false，且不入队（回归 review 发现）。
  {
    const auto response =
        call(exec, *client, "set_tactical_mode", "{mode: retreat, lease_sec: nope}");
    CHECK(response != nullptr);
    if (response != nullptr) {
      CHECK(!response->success);
      CHECK(!response->message.empty());
    }
    CHECK(server.take_commands().empty());
  }

  // 未知 command：返回 success=false。
  {
    const auto response = call(exec, *client, "set_intent", "{}");
    CHECK(response != nullptr);
    if (response != nullptr) {
      CHECK(!response->success);
    }
  }

  rclcpp::shutdown();
  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("all panel service server tests passed\n");
  return 0;
}