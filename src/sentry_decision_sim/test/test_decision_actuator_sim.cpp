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

// 执行端只在延迟结束后结算进世界；重复下发同一累计目标是幂等的。
void test_apply_after_latency_and_idempotent() {
  SimWorld world;
  world.coins = 100;
  world.self_ammo = 10;
  world.event_code |= 1u;  // 已占领己方补给区

  DecisionActuatorSim actuator(1);
  actuator.bind_world(&world, 400);

  DecisionAction action;
  action.kind = DecisionActionKind::kAmmoExchange;
  action.value = 50;  // 协议累计目标值
  action.request_id = 7;
  actuator.send_action(action);
  CHECK(actuator.in_flight() == 1);
  CHECK(world.self_ammo == 10);

  actuator.update();
  CHECK(actuator.in_flight() == 0);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  // 同目标重发（模拟未观测到效果时的重试）：幂等，不再扣币。
  action.request_id = 8;
  actuator.send_action(action);
  actuator.update();
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);
}

// 空动作（task_state 心跳）不应进入裁判结算。
void test_none_action_is_ignored() {
  SimWorld world;
  DecisionActuatorSim actuator(0);
  actuator.bind_world(&world, 400);

  DecisionAction action;
  action.kind = DecisionActionKind::kNone;
  actuator.send_action(action);
  CHECK(actuator.in_flight() == 0);
}

// 非法动作不修改世界。
void test_rejected_action_keeps_world() {
  SimWorld world;
  world.coins = 20;  // 不够买 50 发
  world.event_code |= 1u;

  DecisionActuatorSim actuator(0);
  actuator.bind_world(&world, 400);

  DecisionAction action;
  action.kind = DecisionActionKind::kAmmoExchange;
  action.value = 50;
  actuator.send_action(action);
  actuator.update();
  CHECK(world.self_ammo == 0);
  CHECK(world.coins == 20);
}

// 派发器 + 仿真世界 + 裁判上行：发出 -> 世界结算 -> 上行回填 -> 校验生效。
void test_dispatch_verify_roundtrip() {
  ActionDispatcher dispatcher;
  const TimePoint t0{};

  UpstreamState upstream;
  upstream.valid = true;
  upstream.self_hp = 400;
  upstream.game_status = GameStatus::kRunning;
  upstream.info1.ammo_exchanged_local = 0;

  ActionRequest request;
  request.kind = DecisionActionKind::kAmmoExchange;
  request.target = 10;  // 由 action factory 按累计值折算后的目标
  request.delta = 10;
  request.verify = ActionVerify::kAmmoExchangedLocal;
  request.monotonic = true;

  dispatcher.submit(request);
  const auto outgoing = dispatcher.poll(t0, &upstream);
  CHECK(outgoing.size() == 1);
  CHECK(outgoing[0].value == 10);  // 0 + 10，折算成累计目标

  // 只发一次，不重发。
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());

  // 裁判上行回填累计值 -> 判定生效，不再下发。
  upstream.info1.ammo_exchanged_local = 10;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{100}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
}

}  // namespace

int main() {
  test_apply_after_latency_and_idempotent();
  test_none_action_is_ignored();
  test_rejected_action_keeps_world();
  test_dispatch_verify_roundtrip();
  if (g_failures == 0) {
    std::printf("all decision actuator sim tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
