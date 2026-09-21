#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"

namespace sentry_decision {
namespace {

// 战术模式 -> 期望物理姿态的默认映射：进攻用进攻姿态、防守用防御姿态，其余用移动姿态。
// 规则 5.6.4：开局默认移动姿态；强化姿态不在决策侧指定（仅由裁判 info3 观测）。
SentryStance stance_for(TacticalMode mode) {
  switch (mode) {
    case TacticalMode::kAttack:
      return SentryStance::kAttack;
    case TacticalMode::kDefend:
      return SentryStance::kDefense;
    case TacticalMode::kUnknown:
      // 裁判数据无效时不指挥姿态，交由下位机保持现状。
      return SentryStance::kUnknown;
    default:
      return SentryStance::kMove;
  }
}

StrategicDecision make_decision(TacticalMode mode) {
  return StrategicDecision{mode, stance_for(mode)};
}

}  // namespace

RuleBasedStrategicPolicy::RuleBasedStrategicPolicy(StrategicThresholds thresholds)
    : thresholds_(thresholds) {}

RuleBasedStrategicPolicy RuleBasedStrategicPolicy::from_config(const PolicyConfig& config) {
  StrategicThresholds thresholds;
  if (const auto value = config.number("nav.retreat_hp")) {
    thresholds.retreat_hp = static_cast<int>(*value);
  }
  if (const auto value = config.number("nav.low_ammo")) {
    thresholds.low_ammo = static_cast<int>(*value);
  }
  if (const auto value = config.number("strategic.attack_window_min_remaining")) {
    thresholds.attack_window_min_remaining = static_cast<int>(*value);
  }
  if (const auto value = config.number("strategic.attack_window_max_remaining")) {
    thresholds.attack_window_max_remaining = static_cast<int>(*value);
  }
  return RuleBasedStrategicPolicy(thresholds);
}

StrategicDecision RuleBasedStrategicPolicy::decide(const WorldState& world) const {
  const RefereeState& referee = world.referee;
  if (!referee.valid) {
    return make_decision(TacticalMode::kUnknown);
  }
  if (referee.self_hp <= 0) {
    return make_decision(TacticalMode::kRespawn);
  }
  if (referee.self_hp <= thresholds_.retreat_hp) {
    return make_decision(TacticalMode::kRetreat);
  }
  if (referee.self_ammo <= thresholds_.low_ammo) {
    return make_decision(TacticalMode::kHeal);
  }
  if (referee.our_outpost_hp <= 0) {
    return make_decision(TacticalMode::kDefend);
  }
  if (referee.enemy_outpost_hp > 0 &&
      referee.game_time_remaining >= thresholds_.attack_window_min_remaining &&
      referee.game_time_remaining <= thresholds_.attack_window_max_remaining) {
    return make_decision(TacticalMode::kAttack);
  }
  return make_decision(TacticalMode::kPatrol);
}

}  // namespace sentry_decision
