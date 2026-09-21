#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_io/intervention_convert.hpp"

using namespace sentry_decision;
using namespace sentry_decision_io;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void test_manual_override() {
  InterventionCommand command;
  std::string error;

  CHECK(parse_manual_override(0, "[1.0, 2.0]", 3.0, "debug", &command, &error));
  CHECK(command.kind == InterventionCommand::Kind::kIntent);
  CHECK(command.intent.field == IntentField::kNavGoal);
  CHECK(command.intent.lease == Duration{3000});
  CHECK(std::get<Point2D>(command.intent.value).x == 1.0);
  CHECK(std::get<Point2D>(command.intent.value).y == 2.0);

  CHECK(parse_manual_override(0, "[1, 2, 0.5]", 0.0, "", &command, &error));
  CHECK(std::get<Point2D>(command.intent.value).yaw == 0.5);

  CHECK(parse_manual_override(1, "[0.1, 0.2, 0.3]", 1.0, "", &command, &error));
  CHECK(std::get<Twist>(command.intent.value).wz == 0.3);

  CHECK(parse_manual_override(2, "{ammo: 50, hp: 10, revive: true}", 1.0, "", &command, &error));
  CHECK(std::get<ResourceRequest>(command.intent.value).ammo == 50);
  CHECK(std::get<ResourceRequest>(command.intent.value).revive);

  CHECK(parse_manual_override(3, "retreat", 1.0, "", &command, &error));
  CHECK(std::get<TacticalMode>(command.intent.value) == TacticalMode::kRetreat);
  CHECK(parse_manual_override(3, "4", 1.0, "", &command, &error));
  CHECK(std::get<TacticalMode>(command.intent.value) == TacticalMode::kRetreat);

  CHECK(parse_manual_override(4, "attack", 1.0, "", &command, &error));
  CHECK(std::get<SentryStance>(command.intent.value) == SentryStance::kAttack);
  CHECK(parse_manual_override(4, "2", 1.0, "", &command, &error));
  CHECK(std::get<SentryStance>(command.intent.value) == SentryStance::kDefense);
  CHECK(!parse_manual_override(4, "bogus", 1.0, "", &command, &error));

  CHECK(!parse_manual_override(9, "1", 1.0, "", &command, &error));
  CHECK(!parse_manual_override(0, "not_a_point", 1.0, "", &command, &error));
  CHECK(!parse_manual_override(2, "{ammo: x}", 1.0, "", &command, &error));
  CHECK(!parse_manual_override(0, "[1, 2]", -1.0, "", &command, &error));
}

void test_debug_command() {
  std::vector<InterventionCommand> commands;
  bool list_state = false;
  std::string error;

  CHECK(parse_debug_command("set_world", "{field: self_hp, value: 20}", &commands, &list_state,
                            &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].kind == InterventionCommand::Kind::kWorldOverride);
  CHECK(commands[0].world_field == WorldField::kSelfHp);
  CHECK(commands[0].world_value == 20.0);

  commands.clear();
  CHECK(parse_debug_command("set_intent", "{field: nav_goal, value: [1, 2], lease_sec: 2.0}",
                            &commands, &list_state, &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].intent.field == IntentField::kNavGoal);

  commands.clear();
  CHECK(parse_debug_command("clear_intent", "{field: nav_goal}", &commands, &list_state, &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].kind == InterventionCommand::Kind::kClearIntent);

  commands.clear();
  CHECK(parse_debug_command("set_module", "{module: nav, enabled: false}", &commands, &list_state,
                            &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].module == "nav");
  CHECK(!commands[0].enabled);

  commands.clear();
  CHECK(parse_debug_command("clear_world", "{}", &commands, &list_state, &error));
  CHECK(commands.size() == 6);

  commands.clear();
  CHECK(parse_debug_command("list_state", "", &commands, &list_state, &error));
  CHECK(list_state);
  CHECK(commands.empty());

  CHECK(!parse_debug_command("bogus", "{}", &commands, &list_state, &error));
  CHECK(
      !parse_debug_command("set_world", "{field: nope, value: 1}", &commands, &list_state, &error));
}

}  // namespace

int main() {
  test_manual_override();
  test_debug_command();
  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_intervention_convert passed\n");
  return 0;
}
