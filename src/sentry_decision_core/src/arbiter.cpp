#include "sentry_decision_core/arbiter.hpp"

#include <algorithm>

namespace sentry_decision {
namespace {

// 字段所有权白名单：一个主 owner 与若干允许覆盖者。
struct OwnerRule {
  IntentField field;
  SourceId owner;
  std::vector<SourceId> allowed;
};

const std::vector<OwnerRule>& owner_rules() {
  static const std::vector<OwnerRule> rules = {
      {IntentField::kNavGoal, SourceId::kSkill, {SourceId::kSupervisor, SourceId::kIntervention}},
      {IntentField::kChassisVel,
       SourceId::kRecovery,
       {SourceId::kSupervisor, SourceId::kIntervention}},
      {IntentField::kStance, SourceId::kSkill, {SourceId::kSupervisor, SourceId::kIntervention}},
      {IntentField::kResourceRequest, SourceId::kSkill, {SourceId::kIntervention}},
      {IntentField::kTacticalMode, SourceId::kStrategic, {SourceId::kIntervention}},
  };
  return rules;
}

const OwnerRule* find_rule(IntentField field) {
  for (const auto& rule : owner_rules()) {
    if (rule.field == field) {
      return &rule;
    }
  }
  return nullptr;
}

bool stronger(const Intent& a, const Intent& b) {
  if (a.priority != b.priority) {
    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
  }
  if (a.stamp != b.stamp) {
    return a.stamp > b.stamp;
  }
  return static_cast<int>(a.source) > static_cast<int>(b.source);
}

bool has_matching_value(const Intent& intent) {
  switch (intent.field) {
    case IntentField::kNavGoal:
      return std::holds_alternative<Point2D>(intent.value);
    case IntentField::kChassisVel:
      return std::holds_alternative<Twist>(intent.value);
    case IntentField::kStance:
      return std::holds_alternative<SentryStance>(intent.value);
    case IntentField::kResourceRequest:
      return std::holds_alternative<ResourceRequest>(intent.value);
    case IntentField::kTacticalMode:
      return std::holds_alternative<TacticalMode>(intent.value);
  }
  return false;
}

void apply(const Intent& winner, DecisionOutput& out) {
  switch (winner.field) {
    case IntentField::kNavGoal:
      out.nav_goal = std::get<Point2D>(winner.value);
      break;
    case IntentField::kChassisVel:
      out.cmd_vel = std::get<Twist>(winner.value);
      break;
    case IntentField::kStance:
      out.stance = std::get<SentryStance>(winner.value);
      break;
    case IntentField::kResourceRequest:
      out.resource = std::get<ResourceRequest>(winner.value);
      break;
    case IntentField::kTacticalMode:
      out.tactical_mode = std::get<TacticalMode>(winner.value);
      break;
  }
}

}  // namespace

void IntentArbiter::submit(const Intent& intent) {
  for (auto& existing : intents_) {
    if (existing.source == intent.source && existing.field == intent.field) {
      existing = intent;
      return;
    }
  }
  intents_.push_back(intent);
}

void IntentArbiter::clear_source(SourceId source) {
  intents_.erase(std::remove_if(intents_.begin(), intents_.end(),
                                [source](const Intent& intent) { return intent.source == source; }),
                 intents_.end());
}

void IntentArbiter::clear_all() {
  intents_.clear();
}

bool IntentArbiter::is_valid(const Intent& intent, TimePoint now) {
  if (intent.lease.count() == 0) {
    return true;
  }
  return now <= intent.stamp + intent.lease;
}

bool IntentArbiter::is_owner(IntentField field, SourceId source) {
  const OwnerRule* rule = find_rule(field);
  return rule != nullptr && rule->owner == source;
}

bool IntentArbiter::is_allowed(IntentField field, SourceId source) {
  const OwnerRule* rule = find_rule(field);
  if (rule == nullptr) {
    return false;
  }
  if (rule->owner == source) {
    return true;
  }
  return std::find(rule->allowed.begin(), rule->allowed.end(), source) != rule->allowed.end();
}

ArbiterResult IntentArbiter::resolve(TimePoint now) const {
  ArbiterResult result;
  result.output.stamp = now;

  for (const auto& intent : intents_) {
    if (!is_valid(intent, now)) {
      continue;
    }
    if (!is_allowed(intent.field, intent.source)) {
      result.warnings.push_back("field " + std::to_string(static_cast<int>(intent.field)) +
                                " 被非 owner 来源 " +
                                std::to_string(static_cast<int>(intent.source)) + " 提交");
      continue;
    }
    if (!has_matching_value(intent)) {
      result.warnings.push_back("意图载荷类型与字段不匹配");
    }
  }

  const IntentField fields[] = {IntentField::kNavGoal, IntentField::kChassisVel,
                                IntentField::kStance, IntentField::kResourceRequest,
                                IntentField::kTacticalMode};
  for (IntentField field : fields) {
    const Intent* winner = nullptr;
    std::vector<SourceId> losers;
    for (const auto& intent : intents_) {
      if (intent.field != field || !is_valid(intent, now) ||
          !is_allowed(intent.field, intent.source) || !has_matching_value(intent)) {
        continue;
      }
      if (winner == nullptr || stronger(intent, *winner)) {
        if (winner != nullptr) {
          losers.push_back(winner->source);
        }
        winner = &intent;
      } else {
        losers.push_back(intent.source);
      }
    }
    if (winner != nullptr) {
      apply(*winner, result.output);
      if (!losers.empty()) {
        result.conflicts.push_back(Conflict{field, winner->source, losers});
      }
    }
  }
  return result;
}

}  // namespace sentry_decision
