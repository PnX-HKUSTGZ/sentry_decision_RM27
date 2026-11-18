#pragma once

#include <cstdint>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 裁判系统协议位段解码：把裁判消息中「未解码」的原始整数拆成具名字段。
//
// 本模块只做纯位运算，不依赖 ROS，也不持有状态，因此可在宿主机直接单测。
// 位段依据：RoboMaster 2027 比赛规则手册 V1.0.0 与通信协议 V1.0.0
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

// GameInfo.event_code / event_code_2：2027 0x0101 场地事件数据。
// 字段已按己方视角映射；R/B -> own/enemy 只由 own_color 决定。
struct EventCode {
  bool supply_zone_occupied = false;     // event_data bit 0：己方补给区激活
  std::uint8_t small_energy_status = 0;  // bit 1-2：0 未激活 / 1 已激活 / 2 正在激活
  std::uint8_t big_energy_status = 0;    // bit 3-4：同上

  std::uint8_t own_fort_buff_status = 0;           // bit 5-7 / 8-10：0/1/2/3/4
  std::uint8_t enemy_fort_buff_status = 0;         // 己方 / 对方堡垒增益点激活状态
  std::uint8_t own_base_buff_status = 0;           // bit 11-13 / 14-16
  std::uint8_t enemy_base_buff_status = 0;         // 己方 / 对方基地增益点激活状态
  bool high_point_buff_active = false;             // bit 17：己方制高点增益点激活
  std::uint8_t own_central_highland_status = 0;    // bit 18-19 / 20-21
  std::uint8_t enemy_central_highland_status = 0;  // 己方 / 对方中央高地增益点
  std::uint8_t our_outpost_buff_status = 0;        // bit 22-24 / 25-27
  std::uint8_t enemy_outpost_buff_status = 0;      // 己方 / 对方前哨站增益点

  OutpostControl our_outpost_control = OutpostControl::kNotOwned;    // bit 28-29 / 30-31
  OutpostControl enemy_outpost_control = OutpostControl::kNotOwned;  // 己方 / 对方前哨阵地

  std::uint16_t enemy_dart_last_hit_time_s = 0;  // event_data_2 bit 4-12
  std::uint8_t enemy_dart_last_target = 0;       // event_data_2 bit 13-15

  // 便利字段：己方基地增益点是否由己方 / 双方激活。
  bool base_buff_occupied = false;

  static bool own_active(std::uint8_t status) {
    return status == 1 || status == 3;
  }

  // 本地兑换发弹量（2027 表 5-8）：己方补给区 / 基地增益点 / 前哨站增益点。
  bool local_ammo_exchange_point() const {
    return supply_zone_occupied || base_buff_occupied || own_active(our_outpost_buff_status);
  }
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
EventCode decode_event_code(std::uint32_t event_data, std::uint32_t event_data_2,
                            TeamColor own_color);
SentryInfo1 decode_sentry_info1(std::uint32_t raw);
SentryInfo2 decode_sentry_info2(std::uint16_t raw);
SentryInfo3 decode_sentry_info3(std::uint64_t raw);

}  // namespace sentry_decision
