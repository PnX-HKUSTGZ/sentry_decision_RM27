#pragma once

#include <chrono>
#include <optional>
#include <variant>

namespace sentry_decision {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Duration = std::chrono::milliseconds;

// 意图作用的输出字段。
enum class IntentField {
  kNavGoal,
  kChassisVel,
  kStance,
  kResourceRequest,
  kTacticalMode,
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

enum class SentryStance { kIdle, kMove, kAttack, kDefend };

enum class TacticalMode { kUnknown, kPatrol, kAttack, kDefend, kRetreat, kHeal, kRespawn };

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

struct ResourceRequest {
  int ammo = 0;
  int hp = 0;
  bool revive = false;
};

// 意图载荷；具体用哪一项由 Intent::field 决定。
using IntentValue =
    std::variant<std::monostate, Point2D, Twist, SentryStance, TacticalMode, ResourceRequest>;

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
  SentryStance stance = SentryStance::kIdle;
  ResourceRequest resource{};
  TacticalMode tactical_mode = TacticalMode::kUnknown;
  TimePoint stamp{};
};

}  // namespace sentry_decision
