#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <geometry_msgs/msg/pose.hpp>
#include <iostream>
#include <memory>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <string>
#include <vector>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_core/types.hpp"
#include "sentry_decision_msgs/msg/decision_state.hpp"
#include "sentry_decision_sim/decision_actuator_sim.hpp"
#include "sentry_decision_sim/nav_simulator.hpp"
#include "sentry_decision_sim/scenario.hpp"
#include "sentry_decision_sim/sim_world.hpp"
#include "sentry_interfaces/msg/decision_ack.hpp"
#include "sentry_interfaces/msg/decision_command.hpp"
#include "sentry_interfaces/msg/game_info.hpp"
#include "sentry_interfaces/msg/radar_info.hpp"
#include "sentry_interfaces/msg/sentry_info_offline.hpp"
#include "sentry_interfaces/msg/sentry_info_online.hpp"
#include "sentry_interfaces/msg/team_info.hpp"

namespace {

using sentry_decision::Duration;
using sentry_decision::SteadyClock;
using sentry_decision::TimePoint;
using sentry_decision_sim::DecisionView;
using sentry_decision_sim::Scenario;
using sentry_decision_sim::ScenarioEvent;
using sentry_decision_sim::ScenarioValue;
using sentry_decision_sim::SimWorld;

struct Options {
  double rate_hz = 20.0;
  std::string scenario;
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
    } else if (arg == "--scenario") {
      options.scenario = next("--scenario");
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

sentry_decision::DecisionActionKind kind_from_msg(std::uint8_t kind) {
  using M = sentry_interfaces::msg::DecisionAction;
  using K = sentry_decision::DecisionActionKind;
  switch (kind) {
    case M::KIND_AMMO_EXCHANGE:
      return K::kAmmoExchange;
    case M::KIND_HP_EXCHANGE:
      return K::kHpExchange;
    case M::KIND_FREE_RESURRECT:
      return K::kFreeResurrect;
    case M::KIND_INSTANT_RESURRECT:
      return K::kInstantResurrect;
    case M::KIND_REMOTE_AMMO_EXCHANGE:
      return K::kRemoteAmmoExchange;
    case M::KIND_REMOTE_HP_EXCHANGE:
      return K::kRemoteHpExchange;
    default:
      return K::kNone;
  }
}

sentry_decision::DecisionAction action_from_msg(
    const sentry_interfaces::msg::DecisionCommand& msg) {
  sentry_decision::DecisionAction action;
  action.kind = kind_from_msg(msg.action.kind);
  action.mode = msg.action.mode == sentry_interfaces::msg::DecisionAction::MODE_POLLED
                    ? sentry_decision::ActionMode::kPolled
                    : sentry_decision::ActionMode::kOneShot;
  action.interval = Duration{msg.action.interval_ms};
  action.value = msg.action.value;
  action.request_id = msg.request_id;
  return action;
}

// ROS 侧裁判仿真：发五条上行消息 + odom，提供 NavigateToPose action server 与动作回执，
// 可选按场景时间轴改世界并断言 /decision/state。ROS 无关的仿真状态与解析在 sim 库里。
class RefereeSimNode : public rclcpp::Node {
 public:
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using GoalHandle = rclcpp_action::ServerGoalHandle<NavigateToPose>;

  RefereeSimNode(const Options& options, Scenario scenario, bool has_scenario)
      : Node("sentry_referee_sim"),
        nav_(2.0, 0.2),
        scenario_(std::move(scenario)),
        has_scenario_(has_scenario),
        scenario_start_(SteadyClock::now()) {
    declare_and_create_interfaces();
    if (has_scenario_) {
      for (const auto& item : scenario_.initial_world) {
        std::string error;
        if (!sentry_decision_sim::apply_world_field(&world_, item.first, item.second, &error)) {
          throw std::runtime_error("场景 world: " + error);
        }
      }
    }
    const auto period =
        std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / options.rate_hz));
    timer_ = create_wall_timer(period, [this]() { tick(); });
  }

  int exit_code() const {
    return exit_code_;
  }

 private:
  void declare_and_create_interfaces() {
    const auto game_info_topic =
        declare_parameter<std::string>("game_info_topic", "/sentry/game_info");
    const auto online_info_topic =
        declare_parameter<std::string>("online_info_topic", "/sentry/online_info");
    const auto offline_info_topic =
        declare_parameter<std::string>("offline_info_topic", "/sentry/offline_info");
    const auto team_info_topic =
        declare_parameter<std::string>("team_info_topic", "/sentry/team_info");
    const auto radar_info_topic =
        declare_parameter<std::string>("radar_info_topic", "/sentry/radar_info");
    const auto decision_command_topic =
        declare_parameter<std::string>("decision_command_topic", "/sentry/decision_command");
    const auto decision_ack_topic =
        declare_parameter<std::string>("decision_ack_topic", "/sentry/decision_ack");
    const auto decision_state_topic =
        declare_parameter<std::string>("decision_state_topic", "/decision/state");
    odom_topic_ = declare_parameter<std::string>("odom_topic", "/aft_mapped_to_init");
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    const auto navigate_action =
        declare_parameter<std::string>("navigate_action", "navigate_to_pose");

    game_info_pub_ = create_publisher<sentry_interfaces::msg::GameInfo>(game_info_topic, 10);
    online_info_pub_ =
        create_publisher<sentry_interfaces::msg::SentryInfoOnline>(online_info_topic, 10);
    offline_info_pub_ =
        create_publisher<sentry_interfaces::msg::SentryInfoOffline>(offline_info_topic, 10);
    team_info_pub_ = create_publisher<sentry_interfaces::msg::TeamInfo>(team_info_topic, 10);
    radar_info_pub_ = create_publisher<sentry_interfaces::msg::RadarInfo>(radar_info_topic, 10);
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>(odom_topic_, 10);
    ack_pub_ = create_publisher<sentry_interfaces::msg::DecisionAck>(decision_ack_topic, 10);

    decision_command_sub_ = create_subscription<sentry_interfaces::msg::DecisionCommand>(
        decision_command_topic, 10, [this](sentry_interfaces::msg::DecisionCommand::SharedPtr msg) {
          actuator_.send_action(action_from_msg(*msg));
        });
    decision_state_sub_ = create_subscription<sentry_decision_msgs::msg::DecisionState>(
        decision_state_topic, 10,
        [this](sentry_decision_msgs::msg::DecisionState::SharedPtr msg) { on_decision(*msg); });

    using namespace std::placeholders;
    action_server_ = rclcpp_action::create_server<NavigateToPose>(
        this, navigate_action,
        [this](const rclcpp_action::GoalUUID&, std::shared_ptr<const NavigateToPose::Goal> goal) {
          return handle_goal(*goal);
        },
        [this](const std::shared_ptr<GoalHandle> handle) { return handle_cancel(handle); },
        [this](const std::shared_ptr<GoalHandle> handle) { handle_accepted(handle); });
  }

  rclcpp_action::GoalResponse handle_goal(const NavigateToPose::Goal& goal) {
    SD_LOG_ACT("sim", "收到导航目标 (%.2f, %.2f)", goal.pose.pose.position.x,
               goal.pose.pose.position.y);
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandle>&) {
    nav_.cancel_goal();
    goal_handle_.reset();
    SD_LOG_ACT("sim", "导航目标已取消");
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandle>& handle) {
    goal_handle_ = handle;
    sentry_decision::Point2D goal;
    goal.x = handle->get_goal()->pose.pose.position.x;
    goal.y = handle->get_goal()->pose.pose.position.y;
    nav_.send_goal(goal);
  }

  void on_decision(const sentry_decision_msgs::msg::DecisionState& msg) {
    DecisionView view;
    view.tactical_mode = msg.output.tactical_mode;
    view.has_nav_goal = msg.output.has_nav_goal;
    view.nav_goal_x = msg.output.nav_goal.x;
    view.nav_goal_y = msg.output.nav_goal.y;
    view.has_cmd_vel = msg.output.has_cmd_vel;
    view.resource_ammo = msg.output.resource_ammo;
    view.resource_hp = msg.output.resource_hp;
    view.resource_revive = msg.output.resource_revive;
    last_decision_ = view;
    has_decision_ = true;
  }

  void tick() {
    nav_.update(SteadyClock::now());
    finish_goal_if_done();
    actuator_.update();
    for (const auto& ack : actuator_.take_acks()) {
      publish_ack(ack);
    }
    publish_uplinks();
    publish_odom();
    step_scenario();
  }

  void finish_goal_if_done() {
    if (!goal_handle_) {
      return;
    }
    const sentry_decision::NavState nav = nav_.status();
    auto result = std::make_shared<NavigateToPose::Result>();
    if (nav.reached) {
      SD_LOG_ACT("sim", "导航到达");
      goal_handle_->succeed(result);
      goal_handle_.reset();
    } else if (nav.failed) {
      SD_LOG_WARN("sim", "导航失败");
      goal_handle_->abort(result);
      goal_handle_.reset();
    }
  }

  void publish_ack(const sentry_decision::ActionAck& ack) {
    sentry_interfaces::msg::DecisionAck msg;
    msg.header.stamp = now();
    msg.request_id = ack.request_id;
    msg.accepted = ack.accepted;
    msg.code = ack.code;
    msg.detail = ack.detail;
    ack_pub_->publish(msg);
  }

  void publish_uplinks() {
    const rclcpp::Time stamp = now();

    sentry_interfaces::msg::GameInfo game;
    game.header.stamp = stamp;
    game.header.frame_id = map_frame_;
    game.game_status = static_cast<std::uint8_t>(world_.game_status);
    game.game_time_remaining = static_cast<std::uint16_t>(world_.game_time_remaining);
    game.coin_remaining = static_cast<std::uint16_t>(world_.coins);
    game.event_code = world_.event_code;
    game.detect_color = static_cast<std::uint8_t>(world_.detect_color);
    game.can_rebuild_outpost = world_.can_rebuild_outpost;
    game.manual_point_x = static_cast<float>(world_.manual_point_x);
    game.manual_point_y = static_cast<float>(world_.manual_point_y);
    game.manual_key = static_cast<std::uint8_t>(world_.manual_key);
    game.enemy_outpost_hp = static_cast<std::uint16_t>(world_.enemy_outpost_hp);
    game.enemy_base_hp = static_cast<std::uint16_t>(world_.enemy_base_hp);
    game_info_pub_->publish(game);

    sentry_interfaces::msg::SentryInfoOnline online;
    online.header.stamp = stamp;
    online.self_health = static_cast<std::uint16_t>(world_.self_hp);
    online.bullets_remaining = static_cast<std::uint16_t>(world_.self_ammo);
    online.cooling_value = static_cast<std::uint16_t>(world_.cooling_value);
    online.heat_limit = static_cast<std::uint16_t>(world_.heat_limit);
    online.current_heat = static_cast<std::uint16_t>(world_.current_heat);
    online.speed_monitor_angle = static_cast<float>(world_.speed_monitor_angle);
    online.sentry_info_1 = world_.sentry_info_1;
    online.sentry_info_2 = static_cast<std::uint16_t>(world_.sentry_info_2);
    online.sentry_info_3 = world_.sentry_info_3;
    online.energy_ratio = static_cast<std::uint8_t>(world_.energy_ratio);
    online_info_pub_->publish(online);

    sentry_interfaces::msg::SentryInfoOffline offline;
    offline.header.stamp = stamp;
    offline_info_pub_->publish(offline);

    sentry_interfaces::msg::TeamInfo team;
    team.header.stamp = stamp;
    team.outpost_hp = static_cast<std::uint16_t>(world_.our_outpost_hp);
    team.base_hp = static_cast<std::uint16_t>(world_.base_hp);
    team_info_pub_->publish(team);

    sentry_interfaces::msg::RadarInfo radar;
    radar.header.stamp = stamp;
    radar.enemy_coin_left = static_cast<std::uint16_t>(world_.enemy_coin_left);
    radar.enemy_coin_accumulated = static_cast<std::uint16_t>(world_.enemy_coin_accumulated);
    radar.is_enemy_outpost_sensed = world_.is_enemy_outpost_sensed;
    radar_info_pub_->publish(radar);
  }

  void publish_odom() {
    const sentry_decision::Point2D pose = nav_.pose();
    nav_msgs::msg::Odometry msg;
    msg.header.stamp = now();
    msg.header.frame_id = map_frame_;
    msg.child_frame_id = map_frame_;
    msg.pose.pose.position.x = pose.x;
    msg.pose.pose.position.y = pose.y;
    msg.pose.pose.orientation.z = std::sin(pose.yaw * 0.5);
    msg.pose.pose.orientation.w = std::cos(pose.yaw * 0.5);
    odom_pub_->publish(msg);
  }

  void step_scenario() {
    if (!has_scenario_) {
      return;
    }
    const Duration elapsed =
        std::chrono::duration_cast<Duration>(SteadyClock::now() - scenario_start_);
    while (next_event_ < scenario_.events.size() && scenario_.events[next_event_].at <= elapsed) {
      const ScenarioEvent& event = scenario_.events[next_event_];
      if (event.kind == ScenarioEvent::Kind::kSetWorld) {
        apply_set_world(event);
      } else {
        apply_expect(event);
      }
      ++next_event_;
    }
    if (next_event_ >= scenario_.events.size() && elapsed >= scenario_.end + Duration{500} &&
        !finished_) {
      finish();
    }
  }

  void apply_set_world(const ScenarioEvent& event) {
    for (const auto& item : event.args) {
      std::string error;
      if (!sentry_decision_sim::apply_world_field(&world_, item.first, item.second, &error)) {
        fail("set_world: " + error);
        continue;
      }
    }
    SD_LOG_ACT("sim", "场景 t=%.1fs 应用 set_world", event.at.count() / 1000.0);
  }

  void apply_expect(const ScenarioEvent& event) {
    if (!has_decision_) {
      fail("expect: 尚未收到 /decision/state");
      return;
    }
    for (const auto& item : event.args) {
      std::string error;
      ++expect_total_;
      if (sentry_decision_sim::check_expect(last_decision_, item.first, item.second, tolerance_,
                                            &error)) {
        ++expect_passed_;
        SD_LOG_ACT("sim", "场景 t=%.1fs 断言通过: %s", event.at.count() / 1000.0,
                   item.first.c_str());
      } else {
        fail("expect." + item.first + ": " + error);
      }
    }
  }

  void fail(const std::string& message) {
    failures_.push_back(message);
    SD_LOG_ERROR("sim", "场景失败: %s", message.c_str());
    exit_code_ = 1;
  }

  void finish() {
    finished_ = true;
    if (failures_.empty()) {
      SD_LOG_ACT("sim", "场景 %s 通过（%zu/%zu 断言）", scenario_.name.c_str(), expect_passed_,
                 expect_total_);
    } else {
      SD_LOG_ERROR("sim", "场景 %s 失败（%zu 项）", scenario_.name.c_str(), failures_.size());
    }
    rclcpp::shutdown();
  }

  sentry_decision_sim::NavSimulator nav_;
  sentry_decision_sim::DecisionActuatorSim actuator_{2};
  SimWorld world_;
  Scenario scenario_;
  bool has_scenario_ = false;
  TimePoint scenario_start_{};
  std::size_t next_event_ = 0;
  std::size_t expect_total_ = 0;
  std::size_t expect_passed_ = 0;
  std::vector<std::string> failures_;
  bool finished_ = false;
  int exit_code_ = 0;
  double tolerance_ = 0.05;
  bool has_decision_ = false;
  DecisionView last_decision_;
  std::string odom_topic_;
  std::string map_frame_;

  rclcpp::Publisher<sentry_interfaces::msg::GameInfo>::SharedPtr game_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::SentryInfoOnline>::SharedPtr online_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::SentryInfoOffline>::SharedPtr offline_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::TeamInfo>::SharedPtr team_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::RadarInfo>::SharedPtr radar_info_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::DecisionAck>::SharedPtr ack_pub_;
  rclcpp::Subscription<sentry_interfaces::msg::DecisionCommand>::SharedPtr decision_command_sub_;
  rclcpp::Subscription<sentry_decision_msgs::msg::DecisionState>::SharedPtr decision_state_sub_;
  rclcpp_action::Server<NavigateToPose>::SharedPtr action_server_;
  std::shared_ptr<GoalHandle> goal_handle_;
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

  Scenario scenario;
  bool has_scenario = false;
  if (!options.scenario.empty()) {
    sentry_decision_sim::ScenarioLoadResult loaded =
        sentry_decision_sim::load_scenario(options.scenario);
    if (!loaded.ok()) {
      std::cerr << "场景加载失败: " << options.scenario << "\n";
      for (const auto& error : loaded.errors) {
        std::cerr << "  - " << error << "\n";
      }
      rclcpp::shutdown();
      return 1;
    }
    scenario = std::move(loaded.scenario);
    has_scenario = true;
    SD_LOG_ACT("sim", "加载场景 %s（%zu 事件）", scenario.name.c_str(), scenario.events.size());
  }

  try {
    auto node = std::make_shared<RefereeSimNode>(options, std::move(scenario), has_scenario);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return node->exit_code();
  } catch (const std::exception& ex) {
    std::cerr << "启动失败: " << ex.what() << "\n";
    rclcpp::shutdown();
    return 1;
  }
}
