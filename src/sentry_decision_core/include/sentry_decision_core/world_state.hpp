#pragma once

#include <optional>
#include <vector>

#include "sentry_decision_core/referee_protocol.hpp"
#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 决策可见的世界状态。字段按参考消息（ros_interfaces 的 GameInfo /
// TeamInformation / RadarInfo / SentryInfoOnline / SentryInfoOffline）对齐；
// 每个子状态的有效性由信念层按 stamp + timeout 判定。
//
// 注意：与下位机的通信包（上行 / 下行帧）定义位置尚未确定，本文件只描述
// 「决策视图」的数据契约，不涉及字节流与帧格式。

struct RefereeState {
  TimePoint stamp{};
  bool valid = false;

  // 比赛宏观状态（GameInfo）
  GameStatus game_status = GameStatus::kNotStarted;
  int game_time_remaining = 0;      // 剩余时间，单位 s
  int coins = 0;                    // 己方剩余金币
  int enemy_coins = 0;              // 敌方剩余金币（RadarInfo）
  int enemy_coins_accumulated = 0;  // 敌方累计金币（RadarInfo）
  Point2D manual_point{};           // 手动指定目标点（minimap）
  int manual_key = 0;               // 手动按键
  int detect_color = 0;             // 红蓝方（编码待确认）

  // 建筑血量与经济（GameInfo + TeamInformation）
  int base_hp = 0;
  int our_outpost_hp = 0;
  int enemy_outpost_hp = 0;
  int enemy_base_hp = 0;
  bool can_rebuild_outpost = false;
  bool enemy_outpost_sensed = false;

  // 自身状态（SentryInfoOnline + SentryInfoOffline）
  int self_hp = 0;
  int self_ammo = 0;
  int cooling_value = 0;
  int heat_limit = 0;
  int current_heat = 0;
  int energy_ratio = 0;         // 底盘能量比例
  double gimbal_yaw_deg = 0.0;  // 测速模块朝向，deg，正北为 0
  int lifter_pos = 0;           // 0 上 / 1 下 / 2 中
  bool is_transformable = false;
  double transform_state = 0.0;  // 0~1
  int capacitor_capacity = 0;    // 电容容量百分比
  bool tunnel_yaw_aligned = false;
  double yaw_camera_to_gimbal = 0.0;

  // 裁判协议位段解码结果（详见 referee_protocol.hpp）
  EventCode event{};
  SentryInfo1 info1{};
  SentryInfo2 info2{};
  SentryInfo3 info3{};
};

struct SelfState {
  TimePoint stamp{};
  bool valid = false;
  Point2D pose{};
  double vx = 0.0;
  double vy = 0.0;
  double wz = 0.0;

  // 下位机 IMU 姿态（四元数）。决策暂不使用，先保留。
  Quaternion imu{};
};

struct NavState {
  TimePoint stamp{};
  bool valid = false;
  Point2D pose{};
  std::optional<Point2D> current_goal;
  bool reached = false;
  bool failed = false;
};

struct EnemyRobot {
  int id = 0;
  int hp = 0;
  int allowed_projectile = 0;
  Point2D pose{};
};

struct EnemyState {
  TimePoint stamp{};
  bool valid = false;
  std::optional<Point2D> position;  // 当前锁定目标位置（SentryInfoOffline）
  bool target_valid = false;
  int target_armor_id = 0;
  std::vector<EnemyRobot> enemies;  // 雷达检测到的敌方列表（RadarInfo）
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
