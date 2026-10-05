#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
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
#include "sentry_decision_msgs/msg/world_state.hpp"
#include "sentry_decision_msgs/srv/apply_effect.hpp"
#include "sentry_decision_msgs/srv/debug_command.hpp"
#include "sentry_decision_msgs/srv/set_game_stage.hpp"
#include "sentry_decision_msgs/srv/set_world.hpp"
#include "sentry_decision_sim/decision_actuator_sim.hpp"
#include "sentry_decision_sim/match_stage.hpp"
#include "sentry_decision_sim/nav_simulator.hpp"
#include "sentry_decision_sim/scenario.hpp"
#include "sentry_decision_sim/sim_config.hpp"
#include "sentry_decision_sim/sim_world.hpp"
#include "sentry_interfaces/msg/decision_ack.hpp"
#include "sentry_interfaces/msg/decision_command.hpp"
#include "sentry_interfaces/msg/game_info.hpp"
#include "sentry_interfaces/msg/radar_info.hpp"
#include "sentry_interfaces/msg/sentry_info_offline.hpp"
#include "sentry_interfaces/msg/sentry_info_online.hpp"
#include "sentry_interfaces/msg/team_info.hpp"

#ifndef DEFAULT_SIM_CONFIG_PATH
#define DEFAULT_SIM_CONFIG_PATH "config/sim.yaml"
#endif

namespace {

using sentry_decision::Duration;
using sentry_decision::SteadyClock;
using sentry_decision::TimePoint;
using sentry_decision_sim::DecisionView;
using sentry_decision_sim::Scenario;
using sentry_decision_sim::ScenarioEvent;
using sentry_decision_sim::ScenarioValue;
using sentry_decision_sim::SimConfig;
using sentry_decision_sim::SimWorld;

struct Options {
  double rate_hz = 20.0;
  std::string scenario;
  // 场景时间轴跑完后不退出，保持最后一个世界状态，适合网页面板演示。
  bool hold = false;
  // 仿真参数 yaml；数值默认全部来自该文件（-p 仍可逐项覆盖）。
  std::string sim_config = DEFAULT_SIM_CONFIG_PATH;
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
    } else if (arg == "--hold") {
      options.hold = true;
    } else if (arg == "--sim-config") {
      options.sim_config = next("--sim-config");
    } else if (arg == "--ros-args") {
      // ROS 参数 / 重映射交给 rclcpp，由 declare_parameter 读取；其后参数不再解析。
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

  RefereeSimNode(const Options& options, Scenario scenario, bool has_scenario, SimConfig sim)
      // 自动声明 -p 覆盖：数值参数可能是整数或浮点写法（如 max_hp:=400），
      // 先按覆盖的原生类型声明，再由 declare_number_param 统一读成 double，
      // 避免 declare_parameter<double> 因类型不匹配在启动时抛异常。
      : Node("sentry_referee_sim", rclcpp::NodeOptions()
                                       .allow_undeclared_parameters(true)
                                       .automatically_declare_parameters_from_overrides(true)),
        sim_(std::move(sim)),
        match_(sim_.match),
        scenario_(std::move(scenario)),
        has_scenario_(has_scenario),
        hold_(options.hold),
        scenario_start_(SteadyClock::now()) {
    nav_ = sentry_decision_sim::NavSimulator(sim_.nav_speed, sim_.nav_tolerance);
    declare_and_create_interfaces();
    if (has_scenario_) {
      for (const auto& item : scenario_.initial_world) {
        std::string error;
        if (!sentry_decision_sim::apply_world_field(&world_, item.first, item.second, &error)) {
          throw std::runtime_error("场景 world: " + error);
        }
      }
    }
    // 保存初始世界与位姿，供比赛阶段服务的「重置」恢复。
    initial_world_ = world_;
    start_pose_ = scenario_.start_pose.value_or(nav_.pose());
    nav_.set_pose(start_pose_);
    sim_pose_ = start_pose_;
    // 动作执行端在回执时把兑换结算进仿真世界（扣金币、加血量/发弹量）。
    actuator_.bind_world(&world_, max_hp_);
    last_stage_tick_ = SteadyClock::now();
    last_supply_tick_ = last_stage_tick_;
    last_remote_tick_ = last_stage_tick_;
    last_respawn_tick_ = last_stage_tick_;
    const auto period =
        std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / options.rate_hz));
    timer_ = create_wall_timer(period, [this]() { tick(); });
  }

  int exit_code() const {
    return exit_code_;
  }

 private:
  // 读取数值参数：同时接受整数与浮点写法。带 -p 覆盖时参数已按覆盖类型自动声明，
  // 这里按实际类型取值；没有覆盖时再以 double 默认值声明。
  double declare_number_param(const std::string& name, double fallback) {
    if (!has_parameter(name)) {
      declare_parameter(name, fallback);
      return fallback;
    }
    const rclcpp::Parameter parameter = get_parameter(name);
    if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER) {
      return static_cast<double>(parameter.as_int());
    }
    if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE) {
      return parameter.as_double();
    }
    return fallback;
  }

  void declare_and_create_interfaces() {
    // 是否由本节点提供伪导航（action server + odom）。双仓库联调设 false：
    // 导航由真实 Nav2 提供，本节点只保留裁判仿真，位姿取 /decision/world_state。
    if (has_parameter("provide_nav")) {
      provide_nav_ = get_parameter("provide_nav").as_bool();
    } else {
      provide_nav_ = declare_parameter<bool>("provide_nav", true);
    }

    // 补给区回血 / 血量上限：仿真近似规则 5.2.1（占领补给区每秒回上限血量的
    // 10%，比赛 4 分钟后为 25%）。默认值来自 config/sim.yaml（点位取自其引用的 map）；
    // 命令行 -p 覆盖优先级最高。
    max_hp_ = static_cast<int>(declare_number_param("max_hp", sim_.max_hp));
    supply_center_x_ = declare_number_param("supply_center_x", sim_.supply_x);
    supply_center_y_ = declare_number_param("supply_center_y", sim_.supply_y);
    supply_radius_ = declare_number_param("supply_radius", sim_.supply_radius);
    supply_enter_delay_s_ = declare_number_param("supply_enter_delay_s", sim_.supply_enter_delay_s);
    supply_heal_ratio_ = declare_number_param("supply_heal_ratio", sim_.supply_heal_ratio);
    supply_heal_ratio_late_ =
        declare_number_param("supply_heal_ratio_late", sim_.supply_heal_ratio_late);
    supply_heal_late_after_s_ =
        declare_number_param("supply_heal_late_after_s", sim_.supply_heal_late_after_s);
    // 其余己方增益点区域（半径 <= 0 表示不启用）；用于「本地兑换发弹量」的前置判定。
    base_buff_center_x_ = declare_number_param("base_buff_center_x", sim_.base_x);
    base_buff_center_y_ = declare_number_param("base_buff_center_y", sim_.base_y);
    base_buff_radius_ = declare_number_param("base_buff_radius", sim_.base_radius);
    our_outpost_center_x_ = declare_number_param("our_outpost_center_x", sim_.our_x);
    our_outpost_center_y_ = declare_number_param("our_outpost_center_y", sim_.our_y);
    our_outpost_radius_ = declare_number_param("our_outpost_radius", sim_.our_radius);
    fort_buff_center_x_ = declare_number_param("fort_buff_center_x", sim_.fort_x);
    fort_buff_center_y_ = declare_number_param("fort_buff_center_y", sim_.fort_y);
    fort_buff_radius_ = declare_number_param("fort_buff_radius", sim_.fort_radius);
    // 双仓库重置：回起点导航目标坐标（独立模式不用）。
    start_x_ = declare_number_param("start_x", sim_.start_x);
    start_y_ = declare_number_param("start_y", sim_.start_y);

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
    // 网页面板 / 手动设置比赛阶段（仅仿真）。
    const auto set_game_stage_service =
        declare_parameter<std::string>("set_game_stage_service", "/sentry_sim/set_game_stage");
    set_game_stage_server_ = create_service<sentry_decision_msgs::srv::SetGameStage>(
        set_game_stage_service,
        [this](const std::shared_ptr<sentry_decision_msgs::srv::SetGameStage::Request> request,
               std::shared_ptr<sentry_decision_msgs::srv::SetGameStage::Response> response) {
          handle_set_game_stage(*request, *response);
        });

    // 直接修改仿真世界（网页面板 / 演示用），字段同场景 set_world。
    const auto set_world_service =
        declare_parameter<std::string>("set_world_service", "/sentry_sim/set_world");
    set_world_server_ = create_service<sentry_decision_msgs::srv::SetWorld>(
        set_world_service,
        [this](const std::shared_ptr<sentry_decision_msgs::srv::SetWorld::Request> request,
               std::shared_ptr<sentry_decision_msgs::srv::SetWorld::Response> response) {
          handle_set_world(*request, *response);
        });

    // 具名仿真效果（面板按钮）：步长来自 sim.yaml 的 effects，只改真实世界。
    const auto apply_effect_service =
        declare_parameter<std::string>("apply_effect_service", "/sentry_sim/apply_effect");
    apply_effect_server_ = create_service<sentry_decision_msgs::srv::ApplyEffect>(
        apply_effect_service,
        [this](const std::shared_ptr<sentry_decision_msgs::srv::ApplyEffect::Request> request,
               std::shared_ptr<sentry_decision_msgs::srv::ApplyEffect::Response> response) {
          handle_apply_effect(*request, *response);
        });

    using namespace std::placeholders;
    if (provide_nav_) {
      action_server_ = rclcpp_action::create_server<NavigateToPose>(
          this, navigate_action,
          [this](const rclcpp_action::GoalUUID&, std::shared_ptr<const NavigateToPose::Goal> goal) {
            return handle_goal(*goal);
          },
          [this](const std::shared_ptr<GoalHandle> handle) { return handle_cancel(handle); },
          [this](const std::shared_ptr<GoalHandle> handle) { handle_accepted(handle); });
    } else {
      // 双仓库：真实导航负责下发目标与底盘；本节点只用决策回传的 map 系位姿。
      world_state_topic_ =
          declare_parameter<std::string>("decision_world_state_topic", "/decision/world_state");
      world_state_sub_ = create_subscription<sentry_decision_msgs::msg::WorldState>(
          world_state_topic_, 10, [this](sentry_decision_msgs::msg::WorldState::SharedPtr msg) {
            // 只在位姿有效时更新：TF 未就绪时 self_valid=false 仍会携带旧/默认位姿。
            if (!msg->self_valid) {
              return;
            }
            sim_pose_.x = msg->pos_x;
            sim_pose_.y = msg->pos_y;
            sim_pose_.yaw = msg->yaw;
          });
      // 双仓库下仿真不能瞬移真实机器人：重置时改为下发一个回起点的导航目标。
      nav_reset_client_ = rclcpp_action::create_client<NavigateToPose>(this, navigate_action);
    }
  }

  // 双仓库：请求下发「回起点」导航目标（坐标来自 config/sim.yaml 的 nav_start）。
  // 若 Nav2 尚未就绪，置 pending 由 tick 每拍重试，避免重置请求丢失。
  void request_reset_nav_goal() {
    reset_goal_pending_ = true;
    reset_goal_warned_ = false;
    try_send_reset_nav_goal();
  }

  void try_send_reset_nav_goal() {
    if (!reset_goal_pending_ || !nav_reset_client_) {
      return;
    }
    if (!nav_reset_client_->action_server_is_ready()) {
      if (!reset_goal_warned_) {
        SD_LOG_WARN("sim", "重置：导航 action server 未就绪，稍后重试回起点目标");
        reset_goal_warned_ = true;
      }
      return;
    }
    NavigateToPose::Goal goal;
    goal.pose.header.frame_id = map_frame_;
    goal.pose.header.stamp = now();
    goal.pose.pose.position.x = start_x_;
    goal.pose.pose.position.y = start_y_;
    goal.pose.pose.orientation.w = 1.0;
    nav_reset_client_->async_send_goal(goal);
    reset_goal_pending_ = false;
    SD_LOG_ACT("sim", "重置：下发回起点导航目标 (%.2f, %.2f)", start_x_, start_y_);
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
    // 新目标抢占旧目标：先 abort 旧 handle，避免其悬挂、永远收不到结果。
    if (goal_handle_) {
      if (goal_handle_->is_active()) {
        auto previous = std::make_shared<NavigateToPose::Result>();
        goal_handle_->abort(previous);
      }
      goal_handle_.reset();
    }
    goal_handle_ = handle;
    sentry_decision::Point2D goal;
    goal.x = handle->get_goal()->pose.pose.position.x;
    goal.y = handle->get_goal()->pose.pose.position.y;
    nav_.send_goal(goal);
  }

  void on_decision(const sentry_decision_msgs::msg::DecisionState& msg) {
    DecisionView view;
    view.tactical_mode = msg.output.tactical_mode;
    view.stance = msg.output.stance;
    view.has_nav_goal = msg.output.has_nav_goal;
    view.nav_goal_x = msg.output.nav_goal.x;
    view.nav_goal_y = msg.output.nav_goal.y;
    view.has_cmd_vel = msg.output.has_cmd_vel;
    view.resource_ammo = msg.output.resource_ammo;
    view.resource_hp = msg.output.resource_hp;
    view.resource_revive = msg.output.resource_revive;
    last_decision_ = view;
    has_decision_ = true;
    // 仿真近似：下位机执行决策姿态后，裁判上报的「当前姿态」应与期望一致
    // （真实系统由 MCU 反馈，这里直接把决策姿态回写到 sentry_info_2 的 bit 12-13）。
    if (view.stance != 0) {
      const int stance = view.stance & 0x3;
      world_.sentry_info_2 = (world_.sentry_info_2 & ~(0x3 << 12)) | (stance << 12);
    }
  }

  // 比赛阶段：网页面板 / 手动设置的入口。stage=0 重置到未开始，其余只允许前进。
  void handle_set_game_stage(const sentry_decision_msgs::srv::SetGameStage::Request& request,
                             sentry_decision_msgs::srv::SetGameStage::Response& response) {
    const bool reset_requested = request.stage == 0;
    std::string error;
    if (!match_.set(request.stage, &error)) {
      response.success = false;
      response.stage = static_cast<std::uint8_t>(match_.stage());
      response.remaining_time = match_.remaining_seconds();
      response.message = error;
      SD_LOG_WARN("sim", "比赛阶段设置被拒: %s", error.c_str());
      return;
    }
    if (reset_requested) {
      world_ = initial_world_;
      if (provide_nav_) {
        // 独立仿真：直接瞬移伪导航到起点。
        nav_.set_pose(start_pose_);
        nav_.cancel_goal();
        sim_pose_ = start_pose_;
      } else {
        // 双仓库：仿真不能瞬移真实机器人，改为下发回起点导航目标。
        // 不修改 sim_pose_：仍以 /decision/world_state 的真实位姿判定占领 / 回血，
        // 直到 Nav2 真正把机器人开回起点。
        request_reset_nav_goal();
      }
      actuator_.reset();
      last_supply_tick_ = SteadyClock::now();
      last_remote_tick_ = last_supply_tick_;
      last_respawn_tick_ = last_supply_tick_;
      in_supply_ = false;
      supply_seconds_in_zone_ = 0;
    }
    last_stage_tick_ = SteadyClock::now();
    sync_world_stage();
    response.success = true;
    response.stage = static_cast<std::uint8_t>(match_.stage());
    response.remaining_time = match_.remaining_seconds();
    response.message = "ok";
    SD_LOG_ACT("sim", "比赛阶段 -> %d（剩余 %ds）", static_cast<int>(match_.stage()),
               match_.remaining_seconds());
  }

  // 直接修改仿真世界：字段与场景 set_world 一致，失败给出原因。
  void handle_set_world(const sentry_decision_msgs::srv::SetWorld::Request& request,
                        sentry_decision_msgs::srv::SetWorld::Response& response) {
    ScenarioValue value;
    value.type = ScenarioValue::Type::kNumber;
    value.number = request.value;
    std::string error;
    if (!sentry_decision_sim::apply_world_field(&world_, request.field, value, &error)) {
      response.success = false;
      response.field = request.field;
      response.value = request.value;
      response.message = error;
      SD_LOG_WARN("sim", "设置仿真世界被拒: %s", error.c_str());
      return;
    }
    response.success = true;
    response.field = request.field;
    response.value = request.value;
    response.message = "ok";
    SD_LOG_ACT("sim", "仿真世界 %s = %.2f", request.field.c_str(), request.value);
  }

  // 具名仿真效果：按 sim.yaml 的步长扣减真实世界，供面板模拟赛场事件。
  void handle_apply_effect(const sentry_decision_msgs::srv::ApplyEffect::Request& request,
                           sentry_decision_msgs::srv::ApplyEffect::Response& response) {
    const sentry_decision_sim::EffectResult result =
        sentry_decision_sim::apply_effect(&world_, request.effect, sim_.effects);
    response.success = result.applied;
    response.new_value = result.new_value;
    response.message = result.detail;
    if (result.applied) {
      SD_LOG_ACT("sim", "效果 %s: %s -> %.0f", request.effect.c_str(), result.detail.c_str(),
                 result.new_value);
    } else {
      SD_LOG_WARN("sim", "效果被拒: %s", result.detail.c_str());
    }
  }

  // 把阶段控制器的状态写回 SimWorld；未激活时保留场景设定的 game_status / 时间。
  void sync_world_stage() {
    if (!match_.active()) {
      return;
    }
    world_.game_status = static_cast<int>(match_.stage());
    world_.game_time_remaining = match_.remaining_seconds();
  }

  // 按真实秒推进阶段：计时 + 自检 -> 倒计时 -> 比赛自动切换。
  void step_game_stage() {
    if (!match_.active()) {
      return;
    }
    const TimePoint now = SteadyClock::now();
    while (now - last_stage_tick_ >= Duration{1000}) {
      last_stage_tick_ += Duration{1000};
      match_.tick_second();
    }
    sync_world_stage();
  }

  // 当前位姿来源：独立仿真用伪导航；双仓库联调用 /decision/world_state（map 系）。
  sentry_decision::Point2D current_pose() const {
    return provide_nav_ ? nav_.pose() : sim_pose_;
  }

  // 机器人是否在补给区（以配置的圆心 + 半径判定）。
  bool in_supply_zone() const {
    const sentry_decision::Point2D pose = current_pose();
    return std::hypot(pose.x - supply_center_x_, pose.y - supply_center_y_) <= supply_radius_;
  }

  // 比赛已进行秒数：由「比赛时长 - 当前剩余」得到，时长与 MatchStageController 保持一致。
  int match_elapsed_seconds() const {
    const int total = match_.duration_seconds(sentry_decision_sim::MatchStage::kRunning);
    return std::max(0, total - world_.game_time_remaining);
  }

  // 比赛开始 4 分钟后的回血比例切到 25%（仿真近似规则 5.2.1）。
  double supply_heal_ratio_for_now() const {
    return match_elapsed_seconds() >= static_cast<int>(supply_heal_late_after_s_)
               ? supply_heal_ratio_late_
               : supply_heal_ratio_;
  }

  void apply_supply_second() {
    if (world_.game_status != static_cast<int>(sentry_decision::GameStatus::kRunning)) {
      return;
    }
    const bool inside = in_supply_zone();
    if (inside != in_supply_) {
      in_supply_ = inside;
      supply_seconds_in_zone_ = 0;
      SD_LOG_ACT("sim", "%s补给区", inside ? "进入" : "离开");
    }
    if (!inside) {
      return;
    }
    // 进入后先等检测建立（enter_delay_s，默认 1s），延时满足前的整秒不回血。
    if (sentry_decision_sim::supply_heal_ready(supply_seconds_in_zone_, supply_enter_delay_s_)) {
      const int healed =
          sentry_decision_sim::supply_heal(&world_, max_hp_, supply_heal_ratio_for_now());
      if (healed > 0) {
        SD_LOG_ACT("sim", "补给区回血 +%d -> %d/%d", healed, world_.self_hp, max_hp_);
      }
    }
    ++supply_seconds_in_zone_;
    // 规则 5.3.2：占领补给区时领取累积的免费发弹量（每满 1 分钟 100 发）。
    const int gained =
        sentry_decision_sim::claim_supply_ammo(&world_, match_elapsed_seconds(), true);
    if (gained > 0) {
      SD_LOG_ACT("sim", "补给区免费发弹量 +%d -> %d", gained, world_.self_ammo);
    }
  }

  // 按真实时间推进远程兑换延迟队列（6s 生效）。
  void step_remote_exchanges() {
    const TimePoint now = SteadyClock::now();
    const Duration dt = std::chrono::duration_cast<Duration>(now - last_remote_tick_);
    last_remote_tick_ = now;
    const sentry_decision_sim::RemoteStepResult step =
        sentry_decision_sim::step_pending_remote(&world_, static_cast<int>(dt.count()), max_hp_);
    if (step.ammo_delivered > 0) {
      SD_LOG_ACT("sim", "远程兑换发弹量生效 +%d -> %d", step.ammo_delivered, world_.self_ammo);
    }
    if (step.hp_delivered > 0) {
      SD_LOG_ACT("sim", "远程兑换血量生效 +%d -> %d/%d", step.hp_delivered, world_.self_hp,
                 max_hp_);
    }
    if (step.voided > 0) {
      SD_LOG_WARN("sim", "远程兑换血量因战亡作废（%d 次，金币不返还）", step.voided);
    }
  }

  // 按真实秒推进补给区回血。
  void step_supply() {
    const TimePoint now = SteadyClock::now();
    while (now - last_supply_tick_ >= Duration{1000}) {
      last_supply_tick_ += Duration{1000};
      apply_supply_second();
    }
  }

  // 按真实秒推进复活读条（规则 5.2.2）；读条完成且已确认复活时以 10% 上限血复活。
  void step_respawn() {
    const TimePoint now = SteadyClock::now();
    while (now - last_respawn_tick_ >= Duration{1000}) {
      last_respawn_tick_ += Duration{1000};
      // 非比赛中（未开始 / 准备 / 结算）不推进读条；同时推进时间基线，
      // 否则恢复比赛后会把停表期间一次性补进读条。
      if (world_.game_status != static_cast<int>(sentry_decision::GameStatus::kRunning)) {
        continue;
      }
      if (world_.self_hp > 0) {
        continue;
      }
      // 规则 5.2.2：位于补给区或己方基地血量 <2000 时读条加速（每秒 +4）。
      const bool speed_up = in_supply_zone() || world_.base_hp < 2000;
      const sentry_decision_sim::RespawnStepResult step =
          sentry_decision_sim::step_respawn(&world_, max_hp_, speed_up);
      if (step.revived) {
        SD_LOG_ACT("sim", "复活读条完成（%d/%d）：血量恢复至 %d/%d", step.progress, step.total,
                   world_.self_hp, max_hp_);
      }
    }
  }

  // 按机器人位置生成「占领增益点」状态并写回世界事件位（供上行消息与裁判校验）。
  void update_occupancy() {
    sentry_decision_sim::GainZones zones;
    zones.supply = {supply_center_x_, supply_center_y_, supply_radius_};
    zones.base_buff = {base_buff_center_x_, base_buff_center_y_, base_buff_radius_};
    zones.our_outpost_buff = {our_outpost_center_x_, our_outpost_center_y_, our_outpost_radius_};
    zones.fort_buff = {fort_buff_center_x_, fort_buff_center_y_, fort_buff_radius_};
    const sentry_decision::Point2D pose = current_pose();
    const sentry_decision_sim::Occupancy occupancy =
        sentry_decision_sim::evaluate_occupancy(zones, pose.x, pose.y);
    sentry_decision_sim::apply_occupancy(&world_, occupancy);
  }

  void tick() {
    // 先同步复活状态：战亡即开始读条并导出 info1.can_free_resurrect（供本拍动作校验与上行）。
    sentry_decision_sim::sync_respawn(&world_);
    // 双仓库重置目标：Nav2 未就绪时每拍重试。
    try_send_reset_nav_goal();
    if (provide_nav_) {
      nav_.update(SteadyClock::now());
      finish_goal_if_done();
    }
    update_occupancy();
    actuator_.update();
    for (const auto& ack : actuator_.take_acks()) {
      publish_ack(ack);
    }
    step_remote_exchanges();
    step_game_stage();
    step_supply();
    step_respawn();
    sentry_decision_sim::sync_respawn(&world_);
    publish_uplinks();
    if (provide_nav_) {
      publish_odom();
    }
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
    if (!ack.accepted) {
      // 裁判系统是动作合法性的权威：拒绝时必须留下日志。
      SD_LOG_WARN("sim", "裁判拒绝动作 request_id=%u code=%u %s", ack.request_id,
                  static_cast<unsigned>(ack.code), ack.detail.c_str());
    }
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
      switch (event.kind) {
        case ScenarioEvent::Kind::kSetWorld:
          apply_set_world(event);
          break;
        case ScenarioEvent::Kind::kExpect:
          apply_expect(event);
          break;
      }
      ++next_event_;
    }
    if (!hold_ && next_event_ >= scenario_.events.size() &&
        elapsed >= scenario_.end + Duration{500} && !finished_) {
      finish();
    }
  }

  static std::string scenario_text(const ScenarioValue& value) {
    switch (value.type) {
      case ScenarioValue::Type::kString:
        return value.text;
      case ScenarioValue::Type::kBool:
        return value.boolean ? "true" : "false";
      case ScenarioValue::Type::kNumber:
      default:
        return std::to_string(value.number);
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

  // 结束前终止仍在运行的目标：否则 shutdown 后 action server 析构会对已失效 goal 发 result。
  void abort_active_goal() {
    if (!goal_handle_) {
      return;
    }
    if (goal_handle_->is_active()) {
      auto result = std::make_shared<NavigateToPose::Result>();
      goal_handle_->abort(result);
    }
    goal_handle_.reset();
  }

  void finish() {
    finished_ = true;
    if (failures_.empty()) {
      SD_LOG_ACT("sim", "场景 %s 通过（%zu/%zu 断言）", scenario_.name.c_str(), expect_passed_,
                 expect_total_);
    } else {
      SD_LOG_ERROR("sim", "场景 %s 失败（%zu 项）", scenario_.name.c_str(), failures_.size());
    }
    abort_active_goal();
    rclcpp::shutdown();
  }

  SimConfig sim_;
  sentry_decision_sim::NavSimulator nav_;
  sentry_decision_sim::DecisionActuatorSim actuator_{2};
  SimWorld world_;
  SimWorld initial_world_;
  sentry_decision_sim::MatchStageController match_;
  sentry_decision::Point2D start_pose_{};
  TimePoint last_stage_tick_{};
  TimePoint last_supply_tick_{};
  TimePoint last_remote_tick_{};
  TimePoint last_respawn_tick_{};
  bool in_supply_ = false;
  // 连续在补给区内的整秒数（进入当拍为 0），用于回血进入延时。
  int supply_seconds_in_zone_ = 0;
  // 有效值由 declare_and_create_interfaces() 从 config/sim.yaml 写入；
  // 这里的零值只是构造期占位，不是可调参数。
  int max_hp_ = 0;
  double supply_center_x_ = 0.0;
  double supply_center_y_ = 0.0;
  double supply_radius_ = 0.0;
  double supply_enter_delay_s_ = 0.0;
  double supply_heal_ratio_ = 0.0;
  double supply_heal_ratio_late_ = 0.0;
  double supply_heal_late_after_s_ = 0.0;
  double base_buff_center_x_ = 0.0;
  double base_buff_center_y_ = 0.0;
  double base_buff_radius_ = 0.0;
  double our_outpost_center_x_ = 0.0;
  double our_outpost_center_y_ = 0.0;
  double our_outpost_radius_ = 0.0;
  double fort_buff_center_x_ = 0.0;
  double fort_buff_center_y_ = 0.0;
  double fort_buff_radius_ = 0.0;
  // 双仓库重置：回起点坐标，以及待发送 / 已告警状态。
  double start_x_ = 0.0;
  double start_y_ = 0.0;
  bool reset_goal_pending_ = false;
  bool reset_goal_warned_ = false;
  Scenario scenario_;
  bool has_scenario_ = false;
  bool hold_ = false;
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
  bool provide_nav_ = true;
  sentry_decision::Point2D sim_pose_{};
  std::string world_state_topic_;
  rclcpp::Subscription<sentry_decision_msgs::msg::WorldState>::SharedPtr world_state_sub_;

  rclcpp::Publisher<sentry_interfaces::msg::GameInfo>::SharedPtr game_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::SentryInfoOnline>::SharedPtr online_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::SentryInfoOffline>::SharedPtr offline_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::TeamInfo>::SharedPtr team_info_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::RadarInfo>::SharedPtr radar_info_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sentry_interfaces::msg::DecisionAck>::SharedPtr ack_pub_;
  rclcpp::Subscription<sentry_interfaces::msg::DecisionCommand>::SharedPtr decision_command_sub_;
  rclcpp::Subscription<sentry_decision_msgs::msg::DecisionState>::SharedPtr decision_state_sub_;
  rclcpp::Service<sentry_decision_msgs::srv::SetGameStage>::SharedPtr set_game_stage_server_;
  rclcpp::Service<sentry_decision_msgs::srv::SetWorld>::SharedPtr set_world_server_;
  rclcpp::Service<sentry_decision_msgs::srv::ApplyEffect>::SharedPtr apply_effect_server_;
  rclcpp_action::Server<NavigateToPose>::SharedPtr action_server_;
  // 双仓库：仅用于重置时下发回起点目标。
  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_reset_client_;
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

  // 仿真参数（含增益点 / 补给区 / 速度）全部来自 config/sim.yaml；-p 覆盖在节点内处理。
  SimConfig sim_config;
  std::string sim_path = options.sim_config;
  if (!std::filesystem::exists(sim_path)) {
    const std::string relative = "config/sim.yaml";
    if (std::filesystem::exists(relative)) {
      sim_path = relative;
    }
  }
  {
    std::string sim_error;
    if (!sentry_decision_sim::load_sim_config(sim_path, &sim_config, &sim_error)) {
      std::cerr << "仿真参数加载失败: " << sim_path << "\n  " << sim_error << "\n";
      rclcpp::shutdown();
      return 1;
    }
  }
  SD_LOG_ACT("sim", "仿真参数来自 %s", sim_path.c_str());

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
    auto node =
        std::make_shared<RefereeSimNode>(options, std::move(scenario), has_scenario, sim_config);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return node->exit_code();
  } catch (const std::exception& ex) {
    std::cerr << "启动失败: " << ex.what() << "\n";
    rclcpp::shutdown();
    return 1;
  }
}
