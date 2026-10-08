#include "sentry_decision_core/tactical_override.hpp"

#include "sentry_decision_core/referee_protocol.hpp"

namespace sentry_decision {

void TacticalOverride::set(TacticalMode mode, Duration lease, TimePoint now) {
  mode_ = mode;
  lease_ = lease;
  stamp_ = now;
  set_ = true;
}

void TacticalOverride::clear() {
  set_ = false;
}

bool TacticalOverride::active(TimePoint now) const {
  if (!set_) {
    return false;
  }
  return lease_.count() == 0 || now <= stamp_ + lease_;
}

std::optional<TacticalMode> TacticalOverride::mode(TimePoint now) const {
  if (!active(now)) {
    return std::nullopt;
  }
  return mode_;
}

void apply_tactical_override(const TacticalOverride& override_value, TimePoint now,
                             DecisionContext* context) {
  if (context == nullptr) {
    return;
  }
  // 只在比赛中生效：避免在准备 / 结算阶段伪造战术模式，绕过任务树的比赛阶段门控。
  if (context->world.upstream.game_status != GameStatus::kRunning) {
    return;
  }
  const std::optional<TacticalMode> mode = override_value.mode(now);
  if (!mode.has_value()) {
    return;
  }
  context->strategy.mode = *mode;
  for (auto& intent : context->intents) {
    if (intent.field == IntentField::kTacticalMode && intent.source == SourceId::kStrategic) {
      intent.value = *mode;
    }
  }
}

}  // namespace sentry_decision