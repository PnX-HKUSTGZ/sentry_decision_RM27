#include <cstdio>
#include <string>
#include <vector>

#include "sentry_decision_io/panel_service.hpp"

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

void test_list_state() {
  std::vector<TacticalOverrideCommand> commands;
  bool list_state = false;
  std::string error;
  CHECK(parse_panel_command("list_state", "", &commands, &list_state, &error));
  CHECK(list_state);
  CHECK(commands.empty());
}

void test_set_tactical_mode() {
  std::vector<TacticalOverrideCommand> commands;
  bool list_state = false;
  std::string error;
  CHECK(parse_panel_command("set_tactical_mode", "{mode: retreat, lease_sec: 2.5}", &commands,
                            &list_state, &error));
  CHECK(commands.size() == 1);
  if (commands.size() == 1) {
    CHECK(commands[0].kind == TacticalOverrideCommand::Kind::kSet);
    CHECK(commands[0].mode == TacticalMode::kRetreat);
    CHECK(commands[0].lease == Duration{2500});
  }

  commands.clear();
  CHECK(parse_panel_command("set_tactical_mode", "{mode: 2}", &commands, &list_state, &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].mode == TacticalMode::kAttack);
  CHECK(commands[0].lease == Duration{0});
}

void test_clear_tactical_mode() {
  std::vector<TacticalOverrideCommand> commands;
  bool list_state = false;
  std::string error;
  CHECK(parse_panel_command("clear_tactical_mode", "", &commands, &list_state, &error));
  CHECK(commands.size() == 1);
  CHECK(commands[0].kind == TacticalOverrideCommand::Kind::kClear);
}

void test_rejects_bad_args() {
  std::vector<TacticalOverrideCommand> commands;
  bool list_state = false;
  std::string error;
  CHECK(!parse_panel_command("set_tactical_mode", "{mode: nope}", &commands, &list_state, &error));
  CHECK(!parse_panel_command("set_tactical_mode", "{}", &commands, &list_state, &error));
  CHECK(!parse_panel_command("set_tactical_mode", "{mode: retreat, lease_sec: -1}", &commands,
                             &list_state, &error));
  CHECK(!parse_panel_command("set_intent", "{}", &commands, &list_state, &error));
  CHECK(!error.empty());
}

}  // namespace

int main() {
  test_list_state();
  test_set_tactical_mode();
  test_clear_tactical_mode();
  test_rejects_bad_args();
  if (g_failures == 0) {
    std::printf("all panel service tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}