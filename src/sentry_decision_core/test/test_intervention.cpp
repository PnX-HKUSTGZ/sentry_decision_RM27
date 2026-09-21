#include <cstdio>
#include <variant>

#include "sentry_decision_core/intervention.hpp"

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

Intent make_goal(double x, Duration lease) {
  Intent intent;
  intent.field = IntentField::kNavGoal;
  intent.value = Point2D{x, 0.0, 0.0};
  intent.lease = lease;
  return intent;
}

void test_intent_injection_and_lease() {
  InterventionController controller;
  const TimePoint now{};
  controller.inject(make_goal(3.0, Duration{100}), now);

  auto active = controller.active_intents(now);
  CHECK(active.size() == 1);
  CHECK(active[0].source == SourceId::kIntervention);
  CHECK(active[0].priority == Priority::kIntervention);
  CHECK(std::get<Point2D>(active[0].value).x == 3.0);

  // lease 过期。
  CHECK(controller.active_intents(now + Duration{150}).empty());
}

void test_inject_overrides_same_field() {
  InterventionController controller;
  const TimePoint now{};
  controller.inject(make_goal(1.0, Duration{0}), now);
  controller.inject(make_goal(2.0, Duration{0}), now);
  const auto active = controller.active_intents(now);
  CHECK(active.size() == 1);
  CHECK(std::get<Point2D>(active[0].value).x == 2.0);
}

void test_world_override() {
  InterventionController controller;
  WorldState world;
  world.referee.self_hp = 400;
  world.referee.self_ammo = 100;

  controller.set_world_override(WorldField::kSelfHp, 20);
  const WorldState overridden = controller.apply_world(world);
  CHECK(overridden.referee.self_hp == 20);
  CHECK(overridden.referee.valid);
  // 原世界状态不变。
  CHECK(world.referee.self_hp == 400);
  CHECK(!world.referee.valid);

  controller.clear_world_override(WorldField::kSelfHp);
  CHECK(controller.apply_world(world).referee.self_hp == 400);
}

void test_module_switch() {
  InterventionController controller;
  CHECK(controller.module_enabled("nav"));  // 默认启用
  controller.set_module_enabled("nav", false);
  CHECK(!controller.module_enabled("nav"));
  controller.clear();
  CHECK(controller.module_enabled("nav"));
}

}  // namespace

int main() {
  test_intent_injection_and_lease();
  test_inject_overrides_same_field();
  test_world_override();
  test_module_switch();
  if (g_failures == 0) {
    std::printf("all intervention tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
