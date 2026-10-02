#include <cstdio>

#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_sim/decision_actuator_sim.hpp"
#include "sentry_decision_sim/sim_world.hpp"

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
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
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

// 绑定世界后：回执产生时把兑换结算进世界，金币不足则拒绝且世界不变。
void test_exchange_applied_to_world() {
  SimWorld world;
  world.coins = 100;
  world.self_ammo = 10;
  // 本地兑换发弹量要求占领增益点（规则表 5-8）。
  world.event_code |= 1u;  // 己方补给区已占领
  DecisionActuatorSim actuator(1);
  actuator.bind_world(&world, 400);

  DecisionAction action;
  action.kind = DecisionActionKind::kAmmoExchange;
  action.mode = ActionMode::kOneShot;
  action.value = 50;
  action.request_id = 7;
  actuator.send_action(action);
  actuator.update();
  const auto acks = actuator.take_acks();
  CHECK(acks.size() == 1);
  CHECK(acks[0].accepted);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  world.coins = 0;
  action.request_id = 8;
  actuator.send_action(action);
  actuator.update();
  const auto rejected = actuator.take_acks();
  CHECK(rejected.size() == 1);
  CHECK(!rejected[0].accepted);
  CHECK(world.self_ammo == 60);
}

}  // namespace

int main() {
  test_action_roundtrip();
  test_exchange_applied_to_world();
  if (g_failures == 0) {
    std::printf("all decision actuator sim tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
