#pragma once

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/strategic_policy.hpp"

namespace sentry_decision {

// 规则状态机阈值；可由 PolicyConfig 覆盖，缺失时用默认值。
struct StrategicThresholds {
  int retreat_hp = 50;                    // 低于此血量 -> 撤退
  int low_ammo = 50;                      // 低于此弹量 -> 补给
  int attack_window_min_remaining = 0;    // 允许进攻的剩余时间下界（秒）
  int attack_window_max_remaining = 420;  // 允许进攻的剩余时间上界（秒）
};

// 规则状态机战略策略。
//
// 优先级：复活 > 撤退 > 补给 > 防守 > 进攻 > 巡逻。
// 纯逻辑、无副作用，可脱离行为树单测。
class RuleBasedStrategicPolicy : public StrategicPolicy {
 public:
  explicit RuleBasedStrategicPolicy(StrategicThresholds thresholds = {});

  // 从配置读取 nav.retreat_hp / nav.low_ammo / strategic.attack_window_*，缺失用默认。
  static RuleBasedStrategicPolicy from_config(const PolicyConfig& config);

  StrategicDecision decide(const WorldState& world) const override;

 private:
  StrategicThresholds thresholds_;
};

}  // namespace sentry_decision
