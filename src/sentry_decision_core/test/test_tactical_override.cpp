#include <cstdio>
#include <variant>

#include "sentry_decision_core/tactical_override.hpp"

using namespace sentry_decision;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void test_set_and_expiry() {
  const TimePoint t0{};
  TacticalOverride override_value;
  CHECK(!override_value.active(t0));
  override_value.set(TacticalMode::kRetreat, Duration{100}, t0);
  CHECK(override_value.active(t0));
  CHECK(override_value.mode(t0).has_value());
  CHECK(*override_value.mode(t0) == TacticalMode::kRetreat);
  CHECK(override_value.active(t0 + Duration{100}));
  CHECK(!override_value.active(t0 + Duration{101}));
  CHECK(!override_value.mode(t0 + Duration{101}).has_value());
}

void test_zero_lease_never_expires() {
  const TimePoint t0{};
  TacticalOverride override_value;
  override_value.set(TacticalMode::kAttack, Duration{0}, t0);
  CHECK(override_value.active(t0 + Duration{1000000}));
  CHECK(*override_value.mode(t0 + Duration{1000000}) == TacticalMode::kAttack);
}

void test_clear() {
  const TimePoint t0{};
  TacticalOverride override_value;
  override_value.set(TacticalMode::kHeal, Duration{0}, t0);
  override_value.clear();
  CHECK(!override_value.active(t0));
  CHECK(!override_value.mode(t0).has_value());
}

void test_apply_overrides_context() {
  DecisionContext context;
  context.world.referee.game_status = GameStatus::kRunning;
  StrategicDecision decision;
  decision.mode = TacticalMode::kPatrol;
  decision.stance = SentryStance::kMove;
  context.apply_strategy(decision);

  CHECK(context.strategy.mode == TacticalMode::kPatrol);
  TacticalOverride override_value;
  override_value.set(TacticalMode::kRetreat, Duration{0}, TimePoint{});
  apply_tactical_override(override_value, TimePoint{}, &context);

  CHECK(context.strategy.mode == TacticalMode::kRetreat);
  bool found_mode_intent = false;
  for (const auto& intent : context.intents) {
    if (intent.field != IntentField::kTacticalMode) {
      continue;
    }
    found_mode_intent = true;
    CHECK(intent.source == SourceId::kStrategic);
    CHECK(std::get<TacticalMode>(intent.value) == TacticalMode::kRetreat);
  }
  CHECK(found_mode_intent);
}

void test_apply_without_override_keeps_strategy() {
  DecisionContext context;
  context.world.referee.game_status = GameStatus::kRunning;
  StrategicDecision decision;
  decision.mode = TacticalMode::kDefend;
  context.apply_strategy(decision);
  TacticalOverride override_value;
  apply_tactical_override(override_value, TimePoint{}, &context);
  CHECK(context.strategy.mode == TacticalMode::kDefend);
}

void test_apply_ignored_when_not_running() {
  DecisionContext context;
  StrategicDecision decision;
  decision.mode = TacticalMode::kPatrol;
  context.apply_strategy(decision);
  TacticalOverride override_value;
  override_value.set(TacticalMode::kAttack, Duration{0}, TimePoint{});
  apply_tactical_override(override_value, TimePoint{}, &context);
  CHECK(context.strategy.mode == TacticalMode::kPatrol);
  for (const auto& intent : context.intents) {
    if (intent.field == IntentField::kTacticalMode) {
      CHECK(std::get<TacticalMode>(intent.value) == TacticalMode::kPatrol);
    }
  }
}

}  // namespace

int main() {
  test_set_and_expiry();
  test_zero_lease_never_expires();
  test_clear();
  test_apply_overrides_context();
  test_apply_without_override_keeps_strategy();
  test_apply_ignored_when_not_running();
  if (g_failures == 0) {
    std::printf("all tactical override tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}