#pragma once

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/strategic_policy.hpp"

namespace sentry_decision {

// 规则状态机阈值；可由 PolicyConfig 覆盖，缺失时用默认值。
struct StrategicThresholds {
  int retreat_hp = 50;   // 低于此血量 -> 进入撤退（去补给区）
  int recovery_hp = 90;  // 撤退退出阈值：血量恢复到此值才离开补给区（设为上限血量即回满才走）
  int low_ammo = 50;     // 低于此弹量 -> 补给
  int fort_after_remaining = 180;  // 双方前哨皆毁且剩余 <= 该值 -> 回堡垒防守（否则后方巡逻）
};

// 规则状态机战略策略（还原上一赛季主策略）。
//
// 优先级：复活 > 撤退 > 补给 > 主任务：
//   1) 敌方前哨存活 -> 进攻敌方前哨（kAttack）；
//   2) 敌方前哨被毁、我方前哨存活 -> 高地循环（kPatrol，任务树按前哨条件区分）；
//   3) 双方前哨皆毁 -> 剩余时间 > fort_after_remaining ? 后方巡逻 : 回堡垒防守。
// 撤退带迟滞：self_hp <= retreat_hp 进入，retreat_latched 保持到 self_hp >= recovery_hp。
// 纯逻辑、无副作用（迟滞状态由调用方的 StrategicMemory 持有），可脱离行为树单测。
class RuleBasedStrategicPolicy : public StrategicPolicy {
 public:
  explicit RuleBasedStrategicPolicy(StrategicThresholds thresholds = {});

  // 从配置读取 nav.retreat_hp / nav.recovery_hp / nav.low_ammo / strategic.fort_after_remaining_s。
  static RuleBasedStrategicPolicy from_config(const PolicyConfig& config);

  StrategicDecision decide(const WorldState& world, StrategicMemory* memory) const override;

 private:
  StrategicThresholds thresholds_;
};

}  // namespace sentry_decision
