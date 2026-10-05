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
  if (const auto value = config.number("nav.recovery_hp")) {
    thresholds.recovery_hp = static_cast<int>(*value);
  }
  if (const auto value = config.number("nav.low_ammo")) {
    thresholds.low_ammo = static_cast<int>(*value);
  }
  if (const auto value = config.number("strategic.fort_after_remaining_s")) {
    thresholds.fort_after_remaining = static_cast<int>(*value);
  }
  return RuleBasedStrategicPolicy(thresholds);
}

StrategicDecision RuleBasedStrategicPolicy::decide(const WorldState& world,
                                                   StrategicMemory* memory) const {
  const RefereeState& referee = world.referee;
  if (!referee.valid) {
    return make_decision(TacticalMode::kUnknown);
  }
  // 只有「比赛中」才执行任务；准备 / 自检 / 倒计时 / 结算阶段保持待机。
  if (referee.game_status != GameStatus::kRunning) {
    return make_decision(TacticalMode::kIdle);
  }
  if (referee.self_hp <= 0) {
    if (memory != nullptr) {
      memory->retreat_latched = false;
    }
    return make_decision(TacticalMode::kRespawn);
  }

  // 撤退迟滞：低于 retreat_hp 进入；已进入则保持到血量恢复到
  // recovery_hp，避免刚到阈值就离开补给区。
  bool retreat = referee.self_hp <= thresholds_.retreat_hp;
  if (memory != nullptr) {
    if (memory->retreat_latched) {
      if (referee.self_hp >= thresholds_.recovery_hp) {
        memory->retreat_latched = false;
      } else {
        retreat = true;
      }
    } else if (retreat) {
      memory->retreat_latched = true;
    }
  }
  if (retreat) {
    return make_decision(TacticalMode::kRetreat);
  }

  if (referee.self_ammo <= thresholds_.low_ammo) {
    return make_decision(TacticalMode::kHeal);
  }
  // 规则 1：敌方前哨存活即进攻（不再受进攻时间窗限制）。
  if (referee.enemy_outpost_hp > 0) {
    return make_decision(TacticalMode::kAttack);
  }
  // 规则 2：敌方前哨被毁、我方前哨存活 -> 高地循环（任务树按前哨条件区分高地 / 后方）。
  if (referee.our_outpost_hp > 0) {
    return make_decision(TacticalMode::kPatrol);
  }
  // 规则 3：双方前哨皆毁 -> 时间 < 4min（剩余 > 阈值）后方巡逻，否则回堡垒防守。
  if (referee.game_time_remaining > thresholds_.fort_after_remaining) {
    return make_decision(TacticalMode::kPatrol);
  }
  return make_decision(TacticalMode::kDefend);
}

}  // namespace sentry_decision
