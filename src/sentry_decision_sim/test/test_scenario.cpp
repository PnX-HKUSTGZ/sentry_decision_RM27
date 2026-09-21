#include <cstdio>
#include <string>

#include "sentry_decision_sim/scenario.hpp"
#include "sentry_decision_sim/sim_world.hpp"

using namespace sentry_decision_sim;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

ScenarioValue number(double value) {
  ScenarioValue out;
  out.type = ScenarioValue::Type::kNumber;
  out.number = value;
  return out;
}

ScenarioValue boolean(bool value) {
  ScenarioValue out;
  out.type = ScenarioValue::Type::kBool;
  out.boolean = value;
  return out;
}

ScenarioValue text(const std::string& value) {
  ScenarioValue out;
  out.type = ScenarioValue::Type::kString;
  out.text = value;
  return out;
}

void test_apply_world_field() {
  SimWorld world;
  std::string error;
  CHECK(apply_world_field(&world, "self_hp", number(40), &error));
  CHECK(world.self_hp == 40);
  CHECK(apply_world_field(&world, "can_rebuild_outpost", boolean(true), &error));
  CHECK(world.can_rebuild_outpost);
  CHECK(apply_world_field(&world, "sentry_info_1", number(7), &error));
  CHECK(world.sentry_info_1 == 7u);

  CHECK(!apply_world_field(&world, "not_a_field", number(1), &error));
  CHECK(!error.empty());
  CHECK(!apply_world_field(&world, "self_hp", text("x"), &error));
}

void test_check_expect() {
  DecisionView view;
  view.tactical_mode = 4;
  view.has_nav_goal = true;
  view.nav_goal_x = -5.0;
  view.nav_goal_y = 3.0;

  std::string error;
  CHECK(check_expect(view, "tactical_mode", text("retreat"), 0.05, &error));
  CHECK(check_expect(view, "tactical_mode", number(4), 0.05, &error));
  CHECK(!check_expect(view, "tactical_mode", number(2), 0.05, &error));
  CHECK(!check_expect(view, "tactical_mode", text("bogus"), 0.05, &error));
  CHECK(check_expect(view, "has_nav_goal", boolean(true), 0.05, &error));
  CHECK(check_expect(view, "nav_goal_x", number(-5.0), 0.05, &error));
  CHECK(!check_expect(view, "nav_goal_x", number(1.0), 0.05, &error));
  CHECK(!check_expect(view, "unknown_field", number(0), 0.05, &error));

  view.has_nav_goal = false;
  CHECK(!check_expect(view, "nav_goal_y", number(3.0), 0.05, &error));
  CHECK(check_expect(view, "has_nav_goal", boolean(false), 0.05, &error));
}

void test_load_scenario(const std::string& path) {
  const ScenarioLoadResult loaded = load_scenario(path);
  for (const auto& error : loaded.errors) {
    std::printf("scenario load error: %s\n", error.c_str());
  }
  CHECK(loaded.ok());
  CHECK(loaded.scenario.name == "full_match");
  CHECK(loaded.scenario.events.size() == 7);
  CHECK(loaded.scenario.end == sentry_decision::Duration{10500});
  CHECK(loaded.scenario.initial_world.count("self_hp") == 1);
  if (!loaded.scenario.events.empty()) {
    CHECK(loaded.scenario.events.front().at == sentry_decision::Duration{2000});
    CHECK(loaded.scenario.events.back().at == sentry_decision::Duration{10500});
  }
}

}  // namespace

int main(int argc, char** argv) {
  test_apply_world_field();
  test_check_expect();
  if (argc > 1) {
    test_load_scenario(argv[1]);
  } else {
    std::printf("缺少场景文件参数，跳过 load_scenario 用例\n");
  }

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_scenario passed\n");
  return 0;
}
