#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sentry_decision_sim/scenario.hpp"
#include "sentry_decision_sim/sim_config.hpp"

namespace sentry_decision_sim {

// 远程兑换延迟结算项：确认时已扣金币，规则要求 6 秒后才把效果写入世界。
struct PendingRemoteExchange {
  sentry_decision::DecisionActionKind kind = sentry_decision::DecisionActionKind::kNone;
  int value = 0;         // 兑换次数
  int remaining_ms = 0;  // 距离生效还剩的毫秒
};

// 消息级仿真世界：字段与 sentry_interfaces 上行消息一一对应。
//
// 为什么不复用 core 的 RefereeState：那边存的是解码后的 event / info1 / info2 / info3，
// 反向编码回原始位段会丢失信息、也更容易写错；仿真只需要「发出正确的消息」，
// 因此这里保存原始消息字段。
struct SimWorld {
  // GameInfo
  int game_status = 0;
  int game_time_remaining = 0;
  int coins = 0;
  unsigned int event_code = 0;
  int detect_color = 0;
  bool can_rebuild_outpost = false;
  double manual_point_x = 0.0;
  double manual_point_y = 0.0;
  int manual_key = 0;
  int enemy_outpost_hp = 0;
  int enemy_base_hp = 0;

  // SentryInfoOnline
  int self_hp = 0;
  int self_ammo = 0;
  int cooling_value = 0;
  int heat_limit = 0;
  int current_heat = 0;
  int energy_ratio = 0;
  double speed_monitor_angle = 0.0;
  unsigned int sentry_info_1 = 0;
  // bit 0 = 脱战状态。仿真没有交战模型，默认脱战；可用 set_world {disengaged: 0} 覆盖。
  int sentry_info_2 = 1;
  std::uint64_t sentry_info_3 = 0;

  // ---- 仿真专属的裁判机制状态（不进入上行消息）----
  // 补给区免费发弹量（规则 5.3.2）：比赛每满 1 分钟累积 100 发，占领补给区时领取；
  // 这里记录已领取的整分钟数，未领取部分由 (已进行分钟 - 已领取) 隐式累积。
  int supply_ammo_claimed_minutes = 0;
  // 远程兑换延迟队列（规则 5.3.2 发弹量 / 5.2.1 血量：确认后 6 秒生效）。
  std::vector<PendingRemoteExchange> pending_remote;

  // TeamInfo
  int base_hp = 0;
  int our_outpost_hp = 0;

  // RadarInfo
  int enemy_coin_left = 0;
  int enemy_coin_accumulated = 0;
  bool is_enemy_outpost_sensed = false;
};

// 应用 set_world 的一个字段。未知字段返回 false 并写 error。
bool apply_world_field(SimWorld* world, const std::string& field, const ScenarioValue& value,
                       std::string* error);

// 决策输出的纯视图，供 expect 断言，避免依赖 ROS 消息。
struct DecisionView {
  int tactical_mode = 0;
  int stance = 0;
  bool has_nav_goal = false;
  double nav_goal_x = 0.0;
  double nav_goal_y = 0.0;
  bool has_cmd_vel = false;
  int resource_ammo = 0;
  int resource_hp = 0;
  bool resource_revive = false;
};

// 校验一个 expect 字段：满足返回 true；不满足或字段未知返回 false 并写 error。
bool check_expect(const DecisionView& view, const std::string& field, const ScenarioValue& expected,
                  double tolerance, std::string* error);

// 一次兑换对世界的改动结果，同时供动作回执 detail 使用。
struct ExchangeResult {
  bool accepted = false;
  int amount = 0;      // 实际写入量（发弹量 / 血量）
  int coin_cost = 0;   // 实际扣除金币
  std::string detail;  // 拒绝原因或成功说明
};

// 本地兑换允许发弹量：规则 5.3.1「非远程兑换 10 金币/10 发」，即 1 金币/发。
// value 为请求发数；金币不足或取值非法时 accepted=false。
ExchangeResult exchange_ammo(SimWorld* world, int value);

// 本地兑换血量：仿真简化为 1 金币/1 点，且不超过 max_hp；已满血或金币不足时拒绝。
ExchangeResult exchange_hp(SimWorld* world, int value, int max_hp);

// 补给区回血：按上限血量的 ratio 恢复，返回实际恢复量（已满血返回 0）。
int supply_heal(SimWorld* world, int max_hp, double ratio);

// 进入补给区后是否已满足回血延时：seconds_in_zone 为连续在区内的整秒数（进入当拍为 0）。
bool supply_heal_ready(int seconds_in_zone, double enter_delay_s);

// 一次仿真效果（面板「仿真效果」按钮）的结算结果。
struct EffectResult {
  bool applied = false;
  double new_value = 0.0;  // 结算后该字段的值
  std::string detail;      // 成功说明或未知效果原因
};

// 对仿真世界施加一个具名效果：self_damage / self_ammo_consume / self_death /
// our_outpost_damage / our_outpost_destroy / our_base_damage /
// enemy_outpost_damage / enemy_outpost_destroy / enemy_base_damage。
// 扣血 / 扣弹按 effects 步长做减法并夹到 0；摧毁类直接置 0。未知效果返回 applied=false。
EffectResult apply_effect(SimWorld* world, const std::string& effect, const SimEffects& effects);

// 领取补给区免费发弹量：比赛每满 1 分钟累积 100 发，占领补给区时一次性领取未领取部分。
// match_elapsed_seconds 为比赛已进行秒数；返回本次实际增加的发弹量（0 表示无可领取）。
int claim_supply_ammo(SimWorld* world, int match_elapsed_seconds, bool in_supply);

// 远程兑换延迟结算结果。
struct RemoteStepResult {
  int ammo_delivered = 0;  // 本次到期发放的发弹量
  int hp_delivered = 0;    // 本次到期发放的血量
  int voided = 0;          // 因战亡作废的远程兑换血量次数（金币不返还）
};

// 推进远程兑换延迟：到期项生效。远程血量在 6 秒内战亡则作废，金币不返还。
RemoteStepResult step_pending_remote(SimWorld* world, int elapsed_ms, int max_hp);

// ---- 增益点占领判定（裁判侧）----

// 一个增益点区域（圆）。radius <= 0 表示不启用。
struct GainZone {
  double x = 0.0;
  double y = 0.0;
  double radius = 0.0;

  bool contains(double px, double py) const {
    return radius > 0.0 && ((px - x) * (px - x) + (py - y) * (py - y)) <= radius * radius;
  }
};

// 仿真场上的己方增益点区域。
struct GainZones {
  GainZone supply;            // 己方补给区
  GainZone base_buff;         // 己方基地增益点
  GainZone our_outpost_buff;  // 己方前哨站增益点
  GainZone fort_buff;         // 己方堡垒增益点
};

struct Occupancy {
  bool supply = false;
  bool base_buff = false;
  bool our_outpost_buff = false;
  bool fort_buff = false;

  // 规则表 5-8：本地兑换发弹量要求占领补给区 / 基地 / 前哨站增益点之一。
  bool local_ammo_exchange_point() const {
    return supply || base_buff || our_outpost_buff;
  }
};

// 按机器人位置判定占领了哪些增益点。
Occupancy evaluate_occupancy(const GainZones& zones, double x, double y);

// 把占领状态写回 SimWorld 的 event_code 位段（只改占领相关位，保留其它位）。
void apply_occupancy(SimWorld* world, const Occupancy& occupancy);

// ---- 裁判侧动作前置校验与结算 ----

// 动作结算结果：accepted=false 时世界不变，detail 为拒绝原因（回执 + 日志）。
struct ActionOutcome {
  bool accepted = false;
  std::uint8_t code = 0;  // 0 成功 / 1 参数非法 / 2 前置条件不满足 / 3 金币不足
  std::string detail;
};

// 裁判侧结算一个决策动作（非法动作不修改世界）：
//   kAmmoExchange       本地兑换发弹量：需占领增益点（表 5-8），1 金币/发
//   kHpExchange         兑换血量：需脱战（规则 5.2.1 仅允许远程兑换），1 金币/点（简化）
//   kFreeResurrect      需 info1.can_free_resurrect
//   kInstantResurrect   需 info1.can_instant_resurrect 且金币 >= 所需
//   kRemoteAmmoExchange 需脱战；value = 次数，150 金币/次、每次 +100 发
//   kRemoteHpExchange   需脱战；value = 次数，按规则公式计费、+60% 上限血量
ActionOutcome execute_action(SimWorld* world, const sentry_decision::DecisionAction& action,
                             int max_hp);

}  // namespace sentry_decision_sim
