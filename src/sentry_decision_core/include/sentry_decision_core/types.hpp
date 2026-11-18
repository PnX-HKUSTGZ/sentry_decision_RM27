#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace sentry_decision {

// 红蓝方。Task 层不直接使用；只用于把 2027 R/B 事件位映射成 own/enemy。
enum class TeamColor : std::uint8_t {
  kRed,
  kBlue,
};

// 2027 前哨阵地控制状态（相对己方）。
enum class OutpostControl : std::uint8_t {
  kOwnFull = 0,
  kOwnTemporary = 1,
  kNotOwned = 2,
};

// 下行执行状态：只表示当前在做什么。
enum class TaskState : std::uint8_t {
  kIdle = 0,
  kNavigate = 1,
  kScan = 2,
  kOutpost = 3,
  kResource = 4,
  kHeal = 5,
  kRespawn = 6,
};

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Duration = std::chrono::milliseconds;

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
  // 本地兑换允许发弹量（需占领补给区 / 基地 / 前哨站增益点），单位：发。
  int ammo = 0;
  // 远程兑换允许发弹量次数（需脱战），单位：次。
  int remote_ammo = 0;
  // 远程兑换血量次数（需脱战），单位：次。
  int remote_hp = 0;
  bool revive = false;          // 确认免费复活
  bool instant_revive = false;  // 兑换立即复活（需 can_instant_resurrect 且金币足够）
};

// 决策动作类型（下行给下位机）。
enum class DecisionActionKind {
  kNone,
  kAmmoExchange,  // 本地兑换允许发弹量：需占领增益点（补给区/基地/前哨站）
  kFreeResurrect,       // 确认免费复活：电平位
  kInstantResurrect,    // 兑换立即复活：电平位
  kRemoteAmmoExchange,  // 远程兑换发弹量：需脱战，value = 次数
  kRemoteHpExchange,    // 远程兑换血量：需脱战，value = 次数
};

// 生效判据：只描述“看哪个字段”，具体比较在 action_factory 里实现。
enum class ActionVerify : std::uint8_t {
  kNone,
  kAmmoExchangedLocal,  // 0x020D bit 0-10 累计发弹量
  kRemoteAmmoCount,     // 0x020D bit 11-14 累计远程兑换次数
  kRemoteHpCount,       // 0x020D bit 15-18 累计远程兑换次数
  kAlive,               // 血量从 0 变正
};

// 一个动作请求：语义 + 下发策略。
// 由 Task 构造，经 TaskRuntime 原样带到 dispatcher；dispatcher 不按 kind 猜策略。
struct ActionRequest {
  DecisionActionKind kind = DecisionActionKind::kNone;
  int target = 0;          // 协议绝对目标值（累计值 / 电平位）
  int delta = 0;           // 本次增量，仅用于判断请求是否变化 / 日志
  bool monotonic = false;  // 累计目标，跨请求序列不回退
  ActionVerify verify = ActionVerify::kNone;
  bool level = false;  // 电平动作：完成后补发 value=0
};

// 指令：仲裁后的唯一输出。
struct DecisionOutput {
  std::optional<Point2D> nav_goal;
  std::optional<Twist> cmd_vel;
  ResourceRequest resource{};
  ActionRequest action{};
  TaskState task_state = TaskState::kIdle;
  SentryStance stance = SentryStance::kUnknown;
  TimePoint stamp{};
};

// 下行给下位机的动作（已完成策略解析）。
struct DecisionAction {
  DecisionActionKind kind = DecisionActionKind::kNone;
  int value = 0;
  std::uint32_t request_id = 0;
};

// 一次动作请求序列的最终结果（供日志 / 面板观测）。
struct ActionReport {
  DecisionActionKind kind = DecisionActionKind::kNone;
  std::uint32_t request_id = 0;
  bool effective = false;  // true = 在检测窗口内观测到效果
};

}  // namespace sentry_decision
