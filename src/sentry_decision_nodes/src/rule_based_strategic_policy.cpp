#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"

namespace sentry_decision {

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
    return StrategicDecision{TacticalMode::kUnknown};
  }
  if (referee.self_hp <= 0) {
    return StrategicDecision{TacticalMode::kRespawn};
  }
  if (referee.self_hp <= thresholds_.retreat_hp) {
    return StrategicDecision{TacticalMode::kRetreat};
  }
  if (referee.self_ammo <= thresholds_.low_ammo) {
    return StrategicDecision{TacticalMode::kHeal};
  }
  if (referee.our_outpost_hp <= 0) {
    return StrategicDecision{TacticalMode::kDefend};
  }
  if (referee.enemy_outpost_hp > 0 &&
      referee.game_time_remaining >= thresholds_.attack_window_min_remaining &&
      referee.game_time_remaining <= thresholds_.attack_window_max_remaining) {
    return StrategicDecision{TacticalMode::kAttack};
  }
  return StrategicDecision{TacticalMode::kPatrol};
}

}  // namespace sentry_decision
