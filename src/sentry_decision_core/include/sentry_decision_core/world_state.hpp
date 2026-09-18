#pragma once

#include <optional>
#include <vector>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 以下子状态的最终有效性由信念层按 stamp + timeout 判定。

struct RefereeState {
  TimePoint stamp{};
  bool valid = false;
  int game_progress = 0;  // 比赛阶段 / 剩余时间（占位，待按实际协议细化）
  int self_hp = 0;
  int self_ammo = 0;
  int coins = 0;
  int base_hp = 0;
  int outpost_hp = 0;
  SentryStance robot_stance = SentryStance::kIdle;
};

struct SelfState {
  TimePoint stamp{};
  bool valid = false;
  Point2D pose{};
  double vx = 0.0;
  double vy = 0.0;
  double wz = 0.0;
};

struct NavState {
  TimePoint stamp{};
  bool valid = false;
  Point2D pose{};
  std::optional<Point2D> current_goal;
  bool reached = false;
  bool failed = false;
};

struct EnemyState {
  TimePoint stamp{};
  bool valid = false;
  std::optional<Point2D> position;
};

struct AllyRobot {
  int id = 0;
  Point2D pose{};
  int hp = 0;
};

struct WorldState {
  RefereeState referee;
  SelfState self;
  NavState nav;
  EnemyState enemy;
  std::vector<AllyRobot> allies;
  TimePoint stamp{};
};

}  // namespace sentry_decision
