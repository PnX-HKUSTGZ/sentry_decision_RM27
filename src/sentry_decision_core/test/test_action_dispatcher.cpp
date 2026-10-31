#include <cstdio>
#include <memory>

#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/logging.hpp"

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

ActionDispatcherConfig make_config(Duration verify = Duration{100}, int max_attempts = 3,
                                   Duration revive = Duration{100},
                                   int revive_max_attempts = 0) {
  return ActionDispatcherConfig{verify, max_attempts, revive, revive_max_attempts};
}

DecisionAction make_action(DecisionActionKind kind, int value) {
  DecisionAction action;
  action.kind = kind;
  action.value = value;
  return action;
}

UpstreamState make_upstream() {
  UpstreamState state;
  state.valid = true;
  state.self_hp = 400;
  state.game_status = GameStatus::kRunning;  // 累计值只在比赛中单调递增
  return state;
}

// 生效判据：兑换看 0x020D 累计值，复活看血量从 0 变正。
void test_action_effective() {
  UpstreamState baseline = make_upstream();
  UpstreamState now = baseline;
  now.info1.ammo_exchanged_local = 149;
  CHECK(!action_effective(DecisionActionKind::kAmmoExchange, 150, baseline, now));
  now.info1.ammo_exchanged_local = 150;
  CHECK(action_effective(DecisionActionKind::kAmmoExchange, 150, baseline, now));

  now.info1.remote_ammo_exchange_count = 2;
  CHECK(!action_effective(DecisionActionKind::kRemoteAmmoExchange, 3, baseline, now));
  CHECK(action_effective(DecisionActionKind::kRemoteAmmoExchange, 2, baseline, now));
  now.info1.remote_health_exchange_count = 1;
  CHECK(action_effective(DecisionActionKind::kRemoteHpExchange, 1, baseline, now));

  UpstreamState dead = make_upstream();
  dead.self_hp = 0;
  UpstreamState alive = dead;
  alive.self_hp = 40;
  CHECK(!action_effective(DecisionActionKind::kFreeResurrect, 1, dead, dead));
  CHECK(action_effective(DecisionActionKind::kFreeResurrect, 1, dead, alive));
  CHECK(!action_effective(DecisionActionKind::kFreeResurrect, 1, alive, alive));
  CHECK(action_effective(DecisionActionKind::kInstantResurrect, 1, dead, alive));
}

// 增量 -> 累计目标值：baseline 100 + 50 = 150。
void test_delta_becomes_cumulative_target() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  const auto outgoing = dispatcher.poll(t0, &upstream);
  CHECK(outgoing.size() == 1);
  CHECK(outgoing[0].value == 150);
  CHECK(outgoing[0].request_id != 0);
}

// 生效后不再重发，并产生 effective 报告。
void test_verify_stops_resend() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.info1.remote_ammo_exchange_count = 0;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kRemoteAmmoExchange, 1));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  dispatcher.submit(make_action(DecisionActionKind::kRemoteAmmoExchange, 1));
  CHECK(dispatcher.poll(t0 + Duration{99}, &upstream).empty());

  upstream.info1.remote_ammo_exchange_count = 1;
  dispatcher.submit(make_action(DecisionActionKind::kRemoteAmmoExchange, 1));
  CHECK(dispatcher.poll(t0 + Duration{100}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
  CHECK(report->attempts == 1);
}

// 未生效：按间隔重发同一目标（同一 request_id），达到上限后放弃。
void test_retry_then_give_up() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 0;
  const TimePoint t0{};

  const DecisionAction action = make_action(DecisionActionKind::kAmmoExchange, 50);
  dispatcher.submit(action);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 50);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());

  dispatcher.submit(action);
  const auto second = dispatcher.poll(t0 + Duration{100}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 50);
  CHECK(second[0].request_id == first[0].request_id);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{200}, &upstream).size() == 1);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{300}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(!report->effective);
  CHECK(report->attempts == 3);

  // 放弃后，只要请求还在就不再重试。
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{1000}, &upstream).empty());
}

// 裁判数据无效时暂停：不开始、不判定、不重发。
void test_invalid_upstream_pauses() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.valid = false;
  upstream.info1.ammo_exchanged_local = 0;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  CHECK(dispatcher.poll(t0, &upstream).empty());

  upstream.valid = true;
  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  const auto first = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 50);

  upstream.valid = false;
  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  CHECK(dispatcher.poll(t0 + Duration{500}, &upstream).empty());

  upstream.valid = true;
  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  CHECK(dispatcher.poll(t0 + Duration{600}, &upstream).size() == 1);
}

// 请求值变化 -> 新一段请求：按当时的累计值重新折算，并使用新的 request_id。
void test_value_change_starts_new_series() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 150);

  // 上一段已经生效，累计值变为 150；决策层又请求 20。
  upstream.info1.ammo_exchanged_local = 150;
  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 20));
  const auto second = dispatcher.poll(t0 + Duration{200}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 170);
  CHECK(second[0].request_id != first[0].request_id);
}

// 累计目标只增不减：上行回退到旧值时，下界仍取历史最大值。
void test_target_never_decreases() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 150);

  // 停止提交，条目清除；上行仍是旧值 100。
  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());
  dispatcher.submit(make_action(DecisionActionKind::kAmmoExchange, 50));
  const auto second = dispatcher.poll(t0 + Duration{100}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 200);  // 不回到 150，避免协议要求的非法回退
}

// 免费复活：读条期间无限重发 1；复活后补发 0 清零。
void test_free_resurrect_is_level_until_alive() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  const DecisionAction action = make_action(DecisionActionKind::kFreeResurrect, 1);
  dispatcher.submit(action);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  // 免费复活不限次数：超过普通上限（3）后仍持续置位。
  for (int i = 1; i <= 5; ++i) {
    dispatcher.submit(action);
    const auto again = dispatcher.poll(t0 + Duration{100} * i, &upstream);
    CHECK(again.size() == 1);
    CHECK(again[0].value == 1);
  }

  upstream.self_hp = 40;
  dispatcher.submit(action);
  const auto third = dispatcher.poll(t0 + Duration{600}, &upstream);
  CHECK(third.size() == 1);
  CHECK(third[0].value == 0);  // 清零
  CHECK(third[0].request_id != first[0].request_id);
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
  CHECK(report->attempts == 6);

  // 已清零：请求消失时不再重复补发。
  CHECK(dispatcher.poll(t0 + Duration{650}, &upstream).empty());
}

// 电平动作在请求消失时也要清零。
void test_level_cleared_when_request_disappears() {
  ActionDispatcher dispatcher(make_config());
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  dispatcher.submit(make_action(DecisionActionKind::kFreeResurrect, 1));
  CHECK(dispatcher.poll(t0, &upstream).size() == 1);

  const auto cleared = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(cleared.size() == 1);
  CHECK(cleared[0].value == 0);
}

// 不校验模式（回放 / 无裁判数据）：每次由无到有只发一次。
void test_null_upstream_sends_once() {
  ActionDispatcher dispatcher(make_config());
  const TimePoint t0{};
  const DecisionAction action = make_action(DecisionActionKind::kAmmoExchange, 50);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0, nullptr).size() == 1);
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{500}, nullptr).empty());
}

// 资源请求 -> 动作映射：复活优先，兑换按增量提交。
void test_submit_resource_requests_maps_kinds() {
  const TimePoint t0{};
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 10;
  upstream.info1.remote_ammo_exchange_count = 2;
  upstream.info1.remote_health_exchange_count = 3;

  {
    ActionDispatcher dispatcher(make_config());
    ResourceRequest request;
    request.ammo = 50;
    request.remote_ammo = 1;
    request.remote_hp = 1;
    submit_resource_requests(dispatcher, request);
    const auto out = dispatcher.poll(t0, &upstream);
    CHECK(out.size() == 3);
    CHECK(out[0].kind == DecisionActionKind::kAmmoExchange);
    CHECK(out[0].value == 60);
    CHECK(out[1].kind == DecisionActionKind::kRemoteAmmoExchange);
    CHECK(out[1].value == 3);
    CHECK(out[2].kind == DecisionActionKind::kRemoteHpExchange);
    CHECK(out[2].value == 4);
  }
  {
    ActionDispatcher dispatcher(make_config());
    ResourceRequest request;
    request.instant_revive = true;
    request.revive = true;  // 互斥：立即复活优先
    UpstreamState dead = upstream;
    dead.self_hp = 0;
    submit_resource_requests(dispatcher, request);
    const auto out = dispatcher.poll(t0, &dead);
    CHECK(out.size() == 1);
    CHECK(out[0].kind == DecisionActionKind::kInstantResurrect);
    CHECK(out[0].value == 1);
  }
}

}  // namespace

int main() {
  test_action_effective();
  test_delta_becomes_cumulative_target();
  test_verify_stops_resend();
  test_retry_then_give_up();
  test_invalid_upstream_pauses();
  test_value_change_starts_new_series();
  test_target_never_decreases();
  test_free_resurrect_is_level_until_alive();
  test_level_cleared_when_request_disappears();
  test_null_upstream_sends_once();
  test_submit_resource_requests_maps_kinds();
  if (g_failures == 0) {
    std::printf("all action dispatcher tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
