#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <sstream>
#include <string>

#include "sentry_decision_bringup/config_loader.hpp"
#include "sentry_decision_bringup/runtime_config.hpp"
#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/nav_goal_tracker.hpp"
#include "sentry_decision_core/safety_supervisor.hpp"
#include "sentry_decision_core/task_runner.hpp"
#include "sentry_decision_core/world_model.hpp"
#include "sentry_decision_io/decision_state_publisher.hpp"
#include "sentry_decision_io/panel_service.hpp"
#include "sentry_decision_io/ros_io_node.hpp"

#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "config/profiles.yaml"
#endif

namespace {

using sentry_decision::Duration;
using sentry_decision::SteadyClock;
using sentry_decision::TimePoint;

constexpr Duration kTaskStateHeartbeat{100};

struct Options {
  double rate_hz = 20.0;
  int ticks = 0;
  std::string config = DEFAULT_CONFIG_PATH;
};

Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "参数 " << name << " 缺少取值\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "--rate") {
      options.rate_hz = std::stod(next("--rate"));
    } else if (arg == "--ticks") {
      options.ticks = std::stoi(next("--ticks"));
    } else if (arg == "--config") {
      options.config = next("--config");
    } else if (arg == "--ros-args") {
      break;
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      std::exit(2);
    }
  }
  if (!(options.rate_hz > 0.0) || options.rate_hz > 1000.0) {
    std::cerr << "参数 --rate 必须在 (0, 1000] Hz 之间\n";
    std::exit(2);
  }
  return options;
}

std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

std::string task_params_text(const sentry_decision::TaskCall& call) {
  std::ostringstream out;
  if (const auto* p = std::get_if<sentry_decision::NavigateParams>(&call.params)) {
    out << "target=" << p->target << ", arrive_radius=" << p->arrive_radius;
  } else if (const auto* p = std::get_if<sentry_decision::OutpostParams>(&call.params)) {
    out << "target_side=" << (p->target_side == sentry_decision::Side::kOwn ? "own" : "enemy");
  } else if (const auto* p = std::get_if<sentry_decision::HealParams>(&call.params)) {
    out << "heal_point=" << p->heal_point
        << ", claim_free_ammo=" << (p->claim_free_ammo ? "true" : "false");
  } else if (const auto* p = std::get_if<sentry_decision::ResourceParams>(&call.params)) {
    out << "exchange_point=" << p->exchange_point << ", amount=" << p->amount;
  }
  return out.str();
}

std::string metrics_text(const std::map<std::string, double>& metrics) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3);
  bool first = true;
  for (const auto& item : metrics) {
    if (!first) {
      out << "; ";
    }
    first = false;
    out << item.first << "=" << item.second;
  }
  return out.str();
}

class TaskDecisionNode : public rclcpp::Node {
 public:
  TaskDecisionNode(std::shared_ptr<sentry_decision_io::RosIoNode> io, const Options& options,
                   const sentry_decision::PolicyConfig& config, int max_hp, Duration match_duration)
      : Node("sentry_task_decision"),
        io_(std::move(io)),
        world_model_(*io_, *io_, *io_, sentry_decision_bringup::timeouts_from_config(&config)),
        runner_(config, max_hp, match_duration),
        safety_(sentry_decision_bringup::safety_limits_from_config(&config)),
        state_publisher_(*this),
        config_(&config),
        max_ticks_(options.ticks) {
    panel_service_ = std::make_shared<sentry_decision_io::PanelService>(
        *this, "/decision/debug", [this]() { return state_snapshot_json(); });
    const auto period =
        std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / options.rate_hz));
    timer_ = create_wall_timer(period, [this]() { tick(); });
  }

 private:
  std::string build_task_json() const {
    std::ostringstream out;
    const std::optional<sentry_decision::TaskSnapshot> snapshot = runner_.snapshot();
    out << "\"task\":";
    if (!snapshot.has_value()) {
      out << "null";
    } else {
      const std::string params =
          snapshot->call.has_value() ? task_params_text(*snapshot->call) : std::string{};
      out << "{\"id\":\"" << json_escape(snapshot->task)
          << "\",\"status\":" << static_cast<int>(snapshot->status)
          << ",\"state\":" << static_cast<int>(snapshot->task_state) << ",\"phase\":\""
          << json_escape(snapshot->phase) << "\",\"detail\":\"" << json_escape(snapshot->detail)
          << "\",\"progress\":" << snapshot->progress
          << ",\"elapsed_ms\":" << snapshot->elapsed.count() << ",\"params\":\""
          << json_escape(params) << "\",\"metrics\":\""
          << json_escape(metrics_text(snapshot->metrics)) << "\"}";
    }

    const std::optional<sentry_decision::TaskResult>& previous = runner_.previous_task();
    out << ",\"previous\":";
    if (!previous.has_value()) {
      out << "null";
    } else {
      out << "{\"id\":\"" << json_escape(previous->task)
          << "\",\"status\":" << static_cast<int>(previous->status) << ",\"reason\":\""
          << json_escape(previous->reason) << "\",\"elapsed_ms\":" << previous->elapsed.count()
          << ",\"metrics\":\"" << json_escape(metrics_text(previous->metrics)) << "\"}";
    }

    const std::optional<sentry_decision::TaskSnapshot> resource = runner_.resource_snapshot();
    out << ",\"resource\":";
    if (!resource.has_value()) {
      out << "null";
    } else {
      const std::string params =
          resource->call.has_value() ? task_params_text(*resource->call) : std::string{};
      out << "{\"id\":\"" << json_escape(resource->task)
          << "\",\"status\":" << static_cast<int>(resource->status)
          << ",\"phase\":\"" << json_escape(resource->phase) << "\",\"detail\":\""
          << json_escape(resource->detail) << "\",\"progress\":" << resource->progress
          << ",\"elapsed_ms\":" << resource->elapsed.count() << ",\"params\":\""
          << json_escape(params) << "\",\"metrics\":\""
          << json_escape(metrics_text(resource->metrics)) << "\"}";
    }
    return out.str();
  }

  std::string state_snapshot_json() const {
    std::string task_json;
    {
      std::lock_guard<std::mutex> lock(task_json_mutex_);
      task_json = task_json_;
    }
    std::ostringstream out;
    out << "{\"points\":[";
    bool first = true;
    if (config_ != nullptr) {
      for (const auto& entry : config_->points) {
        if (!first) {
          out << ",";
        }
        first = false;
        double radius = 0.0;
        const auto it = config_->point_radius.find(entry.first);
        if (it != config_->point_radius.end()) {
          radius = it->second;
        }
        out << "{\"name\":\"" << json_escape(entry.first) << "\",\"x\":" << entry.second.x
            << ",\"y\":" << entry.second.y << ",\"r\":" << radius << "}";
      }
    }
    out << "],\"task_state\":" << task_state_json_.load()
        << ",\"safety_emergency\":" << (safety_emergency_json_.load() ? "true" : "false") << ","
        << task_json << "}";
    return out.str();
  }

  void tick() {
    const TimePoint now = SteadyClock::now();
    const sentry_decision::WorldState world = world_model_.snapshot(now);

    runner_.tick(world, now);
    if (const auto result = runner_.take_result(); result.has_value()) {
      SD_LOG_ACT("task", "任务结束 %s status=%d reason=%s", result->task.c_str(),
                 static_cast<int>(result->status), result->reason.c_str());
    }
    if (const auto result = runner_.take_resource_result(); result.has_value()) {
      SD_LOG_ACT("resource", "辅助兑换结束 status=%d reason=%s",
                 static_cast<int>(result->status), result->reason.c_str());
    }
    while (auto epoch = runner_.take_epoch()) {
      SD_LOG_DEBUG("task", "DecisionEpoch reason=%d", static_cast<int>(epoch->reason));
    }
    {
      std::lock_guard<std::mutex> lock(task_json_mutex_);
      task_json_ = build_task_json();
    }

    const sentry_decision::TaskState state = runner_.task_state();
    sentry_decision::DecisionOutput output;
    output.nav_goal = runner_.nav_goal();
    // 主栏动作优先；主栏本拍没有动作时，才让 Resource 辅助栏占用唯一的 action 槽。
    std::optional<sentry_decision::ActionRequest> merged_action = runner_.action();
    if (!merged_action.has_value() ||
        merged_action->kind == sentry_decision::DecisionActionKind::kNone) {
      merged_action = runner_.resource_action();
    }
    output.action = merged_action.value_or(sentry_decision::ActionRequest{});
    output.task_state = state;
    const sentry_decision::SafetyResult safe = safety_.apply(world, output);
    safety_emergency_json_ = safe.emergency;

    const auto nav_step = nav_tracker_.update(safe.output.nav_goal);
    if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kSend &&
        nav_step.goal.has_value()) {
      io_->send_goal(*nav_step.goal);
    } else if (nav_step.decision == sentry_decision::NavGoalTracker::Decision::kCancel) {
      io_->cancel_goal();
    }

    if (safe.output.action.kind != sentry_decision::DecisionActionKind::kNone) {
      dispatcher_.submit(safe.output.action);
    }
    for (const auto& action : dispatcher_.poll(now, &world.upstream)) {
      io_->send_action(action);
    }

    task_state_json_ = static_cast<int>(state);
    if (state != last_task_state_ || now - last_task_state_sent_ >= kTaskStateHeartbeat) {
      io_->send_task_state(state);
      last_task_state_ = state;
      last_task_state_sent_ = now;
    }

    const std::uint32_t tick = tick_count_++;
    state_publisher_.publish(world, safe.output, tick);

    if (max_ticks_ > 0 && tick_count_ >= static_cast<std::uint32_t>(max_ticks_)) {
      rclcpp::shutdown();
    }
  }

  std::shared_ptr<sentry_decision_io::RosIoNode> io_;
  sentry_decision::WorldModel world_model_;
  sentry_decision::TaskRunner runner_;
  sentry_decision::SafetySupervisor safety_;
  sentry_decision_io::DecisionStatePublisher state_publisher_;
  std::shared_ptr<sentry_decision_io::PanelService> panel_service_;
  const sentry_decision::PolicyConfig* config_ = nullptr;
  sentry_decision::NavGoalTracker nav_tracker_;
  sentry_decision::ActionDispatcher dispatcher_;
  sentry_decision::TaskState last_task_state_ = sentry_decision::TaskState::kIdle;
  TimePoint last_task_state_sent_{};
  std::atomic<int> task_state_json_{0};
  std::atomic<bool> safety_emergency_json_{false};
  mutable std::mutex task_json_mutex_;
  std::string task_json_ = "\"task\":null,\"previous\":null";
  std::uint32_t tick_count_ = 0;
  int max_ticks_ = 0;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  const Options options = parse_options(argc, argv);

  auto console = std::make_shared<sentry_decision::ConsoleSink>(false);
  auto& logger = sentry_decision::Logger::instance();
  logger.clear_sinks();
  logger.add_short_sink(console);
  logger.set_short_min_level(sentry_decision::LogLevel::kAct);

  const sentry_decision_bringup::ConfigLoadResult loaded =
      sentry_decision_bringup::load_policy_config(options.config);
  if (!loaded.ok()) {
    std::cerr << "配置加载失败: " << options.config << "\n";
    for (const auto& error : loaded.errors) {
      std::cerr << "  - " << error << "\n";
    }
    rclcpp::shutdown();
    return 1;
  }

  const auto max_hp = loaded.config.number("self.max_hp");
  const auto match_duration_s = loaded.config.number("match.duration_s");
  if (!max_hp.has_value() || !match_duration_s.has_value()) {
    std::cerr << "Task 层需要 self.max_hp 与 match.duration_s 配置\n";
    rclcpp::shutdown();
    return 1;
  }

  rclcpp::NodeOptions io_options;
  io_options.append_parameter_override("map_frame", loaded.config.map_frame);
  auto io = std::make_shared<sentry_decision_io::RosIoNode>(io_options);
  try {
    auto task_node = std::make_shared<TaskDecisionNode>(
        io, options, loaded.config, static_cast<int>(max_hp.value()),
        Duration{static_cast<std::int64_t>(match_duration_s.value() * 1000.0)});
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(io);
    executor.add_node(task_node);
    executor.spin();
  } catch (const std::exception& ex) {
    std::cerr << "启动失败: " << ex.what() << "\n";
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
