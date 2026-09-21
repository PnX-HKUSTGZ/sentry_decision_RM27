#include <cstdio>

#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_sim/decision_actuator_sim.hpp"

using namespace sentry_decision;
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

// 离线 ack 闭环：派发器 -> 模拟执行端 -> 回执 -> 派发器。
void test_action_roundtrip() {
  ActionDispatcher dispatcher;
  DecisionActuatorSim actuator(2);
  const TimePoint t0{};

  DecisionAction action;
  action.kind = DecisionActionKind::kAmmoExchange;
  action.mode = ActionMode::kOneShot;
  action.value = 50;

  dispatcher.submit(action);
  const auto outgoing = dispatcher.poll(t0);
  CHECK(outgoing.size() == 1);
  for (const auto& item : outgoing) {
    actuator.send_action(item);
  }
  CHECK(actuator.in_flight() == 1);

  // 延迟 2 tick 后才产生回执。
  actuator.update();
  CHECK(actuator.take_acks().empty());
  actuator.update();
  const auto acks = actuator.take_acks();
  CHECK(acks.size() == 1);
  CHECK(acks[0].request_id == outgoing[0].request_id);
  CHECK(acks[0].accepted);
  for (const auto& ack : acks) {
    dispatcher.on_ack(ack);
  }

  // ack 之后持续提交同值：不重发。
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{50}).empty());
}

}  // namespace

int main() {
  test_action_roundtrip();
  if (g_failures == 0) {
    std::printf("all decision actuator sim tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
