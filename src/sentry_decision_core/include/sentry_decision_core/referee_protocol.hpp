#pragma once

#include <cstdint>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 裁判系统协议位段解码：把裁判消息中「未解码」的原始整数拆成具名字段。
//
// 本模块只做纯位运算，不依赖 ROS，也不持有状态，因此可在宿主机直接单测。
// 位段依据：RoboMaster 2026 比赛规则手册 V2.2.0 与通信协议 V2.0.0
// （0x0001 比赛状态、0x0101 场地事件、0x020D 哨兵决策信息同步）。
// 协议变更时先改这里，再同步 io 适配器。

// 比赛阶段，来自 GameInfo.game_status（0x0001 的 game_progress）。
enum class GameStatus : std::uint8_t {
  kNotStarted = 0,   // 未开始比赛
  kPreparation = 1,  // 准备阶段
  kSelfCheck = 2,    // 15s 裁判系统自检
  kCountdown = 3,    // 5s 倒计时
  kRunning = 4,      // 比赛中
  kSettling = 5,     // 比赛结算中
  kUnknown = 0xFF,   // 未识别取值，便于上层判定协议异常
};

// GameInfo.event_code：0x0101 场地事件数据（uint32）。
struct EventCode {
  bool supply_zone_occupied = false;         // bit 0：己方补给区已占领
  bool supply_zone_occupied_rmul = false;    // bit 2：己方补给区已占领（仅 RMUL）
  std::uint8_t small_energy_status = 0;      // bit 3-4：0 未激活 / 1 已激活 / 2 正在激活
  std::uint8_t big_energy_status = 0;        // bit 5-6：0 未激活 / 1 已激活 / 2 正在激活
  std::uint8_t central_highland_status = 0;  // bit 7-8：1 己方 / 2 对方
  std::uint8_t trapezoid_highland_status = 0;  // bit 9-10：1 已占领
  std::uint16_t enemy_dart_last_hit_time_s = 0;  // bit 11-19：对方飞镖最后击中时间（0-420s）
  std::uint8_t enemy_dart_last_target = 0;  // bit 20-22：1 前哨 / 2-5 基地各类目标
  std::uint8_t center_buff_status = 0;  // bit 23-24：0/1 己方/2 对方/3 双方（仅 RMUL）
  std::uint8_t fort_occupation_status = 0;   // bit 25-26：己方堡垒增益点占领状态
  std::uint8_t our_outpost_buff_status = 0;  // bit 27-28：己方前哨站增益点占领状态
  bool base_buff_occupied = false;           // bit 29：己方基地增益点已占领
};

// SentryInfoOnline.sentry_info_1：兑换与复活信息。
struct SentryInfo1 {
  std::uint16_t ammo_exchanged_local = 0;  // bit 0-10：本地已成功兑换的允许发弹量
  std::uint8_t remote_ammo_exchange_count = 0;    // bit 11-14：远程兑换发弹量次数
  std::uint8_t remote_health_exchange_count = 0;  // bit 15-18：远程兑换血量次数
  bool can_free_resurrect = false;                // bit 19：是否可确认免费复活
  bool can_instant_resurrect = false;             // bit 20：是否可兑换立即复活
  std::uint16_t instant_resurrect_cost = 0;       // bit 21-30：立即复活所需金币
};

// SentryInfoOnline.sentry_info_2：脱战、可兑换弹量、姿态与能量机关。
// 姿态为 2026 规则 5.6.4 的有效机制，决策据此选择并输出 SentryStance。
struct SentryInfo2 {
  bool disengaged = false;                       // bit 0：脱战状态
  std::uint16_t remaining_ammo_exchange = 0;     // bit 1-11：17mm 剩余可兑换发弹量
  SentryStance stance = SentryStance::kUnknown;  // bit 12-13：1 进攻 / 2 防御 / 3 移动
  bool can_activate_energy = false;              // bit 14：能否进入能量机关激活状态
  bool stance_enhanced = false;                  // bit 15：是否强化姿态
};

// SentryInfoOnline.sentry_info_3：各姿态剩余可持续时长（秒）。
struct SentryInfo3 {
  std::uint8_t attack_stance_remaining_s = 0;     // bit 0-7：进攻姿态弱化前剩余时长
  std::uint8_t defense_stance_remaining_s = 0;    // bit 8-15：防御姿态弱化前剩余时长
  std::uint8_t move_stance_remaining_s = 0;       // bit 16-23：移动姿态弱化前剩余时长
  std::uint8_t attack_enhanced_remaining_s = 0;   // bit 32-39：强化进攻姿态剩余时长
  std::uint8_t defense_enhanced_remaining_s = 0;  // bit 40-47：强化防御姿态剩余时长
  std::uint8_t move_enhanced_remaining_s = 0;     // bit 48-55：强化移动姿态剩余时长
};

GameStatus decode_game_status(std::uint8_t raw);
// 裁判姿态位段 -> 具名姿态（1 进攻 / 2 防御 / 3 移动，其余 kUnknown）。
SentryStance decode_stance(std::uint8_t raw);
EventCode decode_event_code(std::uint32_t raw);
SentryInfo1 decode_sentry_info1(std::uint32_t raw);
SentryInfo2 decode_sentry_info2(std::uint16_t raw);
SentryInfo3 decode_sentry_info3(std::uint64_t raw);

}  // namespace sentry_decision
