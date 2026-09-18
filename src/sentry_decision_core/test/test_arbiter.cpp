#include <cstdio>

#include "sentry_decision_core/arbiter.hpp"

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

Intent make_nav(SourceId source, Priority priority, TimePoint stamp, double x,
                Duration lease = Duration{0}) {
  Intent intent;
  intent.field = IntentField::kNavGoal;
  intent.source = source;
  intent.priority = priority;
  intent.stamp = stamp;
  intent.lease = lease;
  intent.value = Point2D{x, 0.0, 0.0};
  return intent;
}

void test_owner_rules() {
  CHECK(IntentArbiter::is_owner(IntentField::kNavGoal, SourceId::kSkill));
  CHECK(IntentArbiter::is_allowed(IntentField::kNavGoal, SourceId::kSupervisor));
  CHECK(!IntentArbiter::is_allowed(IntentField::kNavGoal, SourceId::kRecovery));
}

void test_priority_wins() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 1.0));
  arbiter.submit(make_nav(SourceId::kIntervention, Priority::kIntervention, t0, 2.0));
  const ArbiterResult result = arbiter.resolve(t0);
  CHECK(result.output.nav_goal.has_value());
  CHECK(result.output.nav_goal->x == 2.0);
  CHECK(result.conflicts.size() == 1);
}

void test_lease_expiry() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 1.0, Duration{100}));
  CHECK(arbiter.resolve(t0).output.nav_goal.has_value());
  CHECK(!arbiter.resolve(t0 + Duration{101}).output.nav_goal.has_value());
}

void test_replace_same_source() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 1.0));
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 3.0));
  CHECK(arbiter.resolve(t0).output.nav_goal->x == 3.0);
}

void test_non_owner_warning() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kRecovery, Priority::kRecovery, t0, 9.0));
  const ArbiterResult result = arbiter.resolve(t0);
  CHECK(!result.output.nav_goal.has_value());
  CHECK(result.warnings.size() == 1);
}

void test_clear_source() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 1.0));
  arbiter.clear_source(SourceId::kSkill);
  CHECK(!arbiter.resolve(t0).output.nav_goal.has_value());
}

void test_type_mismatch() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  Intent bad;
  bad.field = IntentField::kNavGoal;
  bad.source = SourceId::kSkill;
  bad.priority = Priority::kTactical;
  bad.stamp = t0;
  bad.value = Twist{1.0, 0.0, 0.0};
  arbiter.submit(bad);
  const ArbiterResult result = arbiter.resolve(t0);
  CHECK(!result.output.nav_goal.has_value());
  CHECK(!result.warnings.empty());
}

void test_fresher_wins_on_tie() {
  const TimePoint t0{};
  IntentArbiter arbiter;
  arbiter.submit(make_nav(SourceId::kSkill, Priority::kTactical, t0, 1.0));
  arbiter.submit(make_nav(SourceId::kIntervention, Priority::kIntervention, t0, 2.0));
  CHECK(arbiter.resolve(t0).output.nav_goal->x == 2.0);
}

}  // namespace

int main() {
  test_owner_rules();
  test_priority_wins();
  test_lease_expiry();
  test_replace_same_source();
  test_non_owner_warning();
  test_clear_source();
  test_type_mismatch();
  test_fresher_wins_on_tie();
  if (g_failures == 0) {
    std::printf("all core arbiter tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
