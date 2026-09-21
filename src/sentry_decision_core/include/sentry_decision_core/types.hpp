#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace sentry_decision {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Duration = std::chrono::milliseconds;

// 意图作用的输出字段。
enum class IntentField {
  kNavGoal,
  kChassisVel,
  kResourceRequest,
  kTacticalMode,
  kStance,
};

// 意图来源。枚举顺序也用于同优先级、同时间戳时的确定性 tie-break。
enum class SourceId {
  kStrategic,
  kMission,
  kSkill,
  kRecovery,
  kIntervention,
  kSupervisor,
};

// 优先级带（数值越大越强），与 ARCHITECTURE 第 9.2 节一致。
enum class Priority : int {
  kDefault = 0,
  kTactical = 1,
  kRecovery = 2,
  kIntervention = 3,
  kSafety = 4,
};

enum class TacticalMode { kUnknown, kPatrol, kAttack, kDefend, kRetreat, kHeal, kRespawn };

// 哨兵物理姿态（2026 规则 5.6.4）。取值与裁判 SentryInfo2.stance 位段一致：
// 1 进攻 / 2 防御 / 3 移动；0 表示未知（消息未给或不在比赛中）。
enum class SentryStance : std::uint8_t {
  kUnknown = 0,
  kAttack = 1,
  kDefense = 2,
  kMove = 3,
};

struct Point2D {
  double x = 0.0;
  double y = 0.0;
  double yaw = 0.0;
};

struct Twist {
  double vx = 0.0;
  double vy = 0.0;
  double wz = 0.0;
};

// 下位机 IMU 姿态（四元数）。决策当前暂不使用，先保留以备后续需求。
struct Quaternion {
  double w = 1.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct ResourceRequest {
  int ammo = 0;
  int hp = 0;
  bool revive = false;
};

// 意图载荷；具体用哪一项由 Intent::field 决定。
using IntentValue =
    std::variant<std::monostate, Point2D, Twist, TacticalMode, ResourceRequest, SentryStance>;

// 意图：一次请求，不保证最终生效。
struct Intent {
  IntentField field = IntentField::kNavGoal;
  SourceId source = SourceId::kSkill;
  Priority priority = Priority::kDefault;
  TimePoint stamp{};
  // lease.count() == 0 表示不自动过期，只在被替换或清空时失效。
  Duration lease{0};
  IntentValue value{};
};

// 指令：仲裁后的唯一输出。
struct DecisionOutput {
  std::optional<Point2D> nav_goal;
  std::optional<Twist> cmd_vel;
  ResourceRequest resource{};
  TacticalMode tactical_mode = TacticalMode::kUnknown;
  SentryStance stance = SentryStance::kUnknown;
  TimePoint stamp{};
};

// 决策动作类型（下行给下位机）。
enum class DecisionActionKind {
  kNone,
  kAmmoExchange,        // 本地兑换允许发弹量，value = 数量
  kHpExchange,          // 本地兑换血量，value = 数量
  kFreeResurrect,       // 确认免费复活
  kInstantResurrect,    // 兑换立即复活
  kRemoteAmmoExchange,  // 远程兑换发弹量，value = 次数
  kRemoteHpExchange,    // 远程兑换血量，value = 次数
};

// 动作发送模式：配置动作时必须显式选择。
enum class ActionMode {
  kOneShot,  // 边沿触发，执行一次即完成
  kPolled,   // 轮询，按 interval 重发，以最新值为准
};

// 一个决策动作。
struct DecisionAction {
  DecisionActionKind kind = DecisionActionKind::kNone;
  ActionMode mode = ActionMode::kOneShot;
  Duration interval{0};  // 仅 kPolled 使用
  int value = 0;
  std::uint32_t request_id = 0;
};

// 下位机对某个动作的执行回执（ROS 无关版本，对应 DecisionAck 消息）。
struct ActionAck {
  std::uint32_t request_id = 0;
  bool accepted = false;
  std::uint8_t code = 0;
  std::string detail;
};

}  // namespace sentry_decision
