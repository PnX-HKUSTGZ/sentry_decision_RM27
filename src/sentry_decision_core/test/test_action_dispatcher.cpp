#include <cstdio>

#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/action_factory.hpp"

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

UpstreamState make_upstream() {
  UpstreamState state;
  state.valid = true;
  state.self_hp = 400;
  state.game_status = GameStatus::kRunning;
  return state;
}

ActionRequest make_request(DecisionActionKind kind, int target, int delta, ActionVerify verify,
                           bool monotonic = true, bool level = false) {
  ActionRequest request;
  request.kind = kind;
  request.target = target;
  request.delta = delta;
  request.verify = verify;
  request.monotonic = monotonic;
  request.level = level;
  return request;
}

void test_verify_action() {
  UpstreamState baseline = make_upstream();
  UpstreamState now = baseline;
  now.info1.ammo_exchanged_local = 149;
  CHECK(!verify_action(ActionVerify::kAmmoExchangedLocal, 150, baseline, now));
  now.info1.ammo_exchanged_local = 150;
  CHECK(verify_action(ActionVerify::kAmmoExchangedLocal, 150, baseline, now));

  now.info1.remote_ammo_exchange_count = 2;
  CHECK(!verify_action(ActionVerify::kRemoteAmmoCount, 3, baseline, now));
  CHECK(verify_action(ActionVerify::kRemoteAmmoCount, 2, baseline, now));
  now.info1.remote_health_exchange_count = 1;
  CHECK(verify_action(ActionVerify::kRemoteHpCount, 1, baseline, now));

  UpstreamState dead = make_upstream();
  dead.self_hp = 0;
  UpstreamState alive = dead;
  alive.self_hp = 40;
  CHECK(!verify_action(ActionVerify::kAlive, 1, dead, dead));
  CHECK(verify_action(ActionVerify::kAlive, 1, dead, alive));
  CHECK(!verify_action(ActionVerify::kAlive, 1, alive, alive));
}

void test_ammo_factory() {
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;

  const ActionRequest ok = make_ammo_exchange(upstream, 50);
  CHECK(ok.kind == DecisionActionKind::kAmmoExchange);
  CHECK(ok.delta == 50);
  CHECK(ok.target == 150);
  CHECK(ok.verify == ActionVerify::kAmmoExchangedLocal);
  CHECK(ok.monotonic);

  const ActionRequest rounded = make_ammo_exchange(upstream, 55);
  CHECK(rounded.delta == 50);
  CHECK(rounded.target == 150);

  upstream.info1.ammo_exchanged_local = 990;
  const ActionRequest capped = make_ammo_exchange(upstream, 50);
  CHECK(capped.target == 1000);
}

void test_remote_factory() {
  UpstreamState upstream = make_upstream();
  upstream.info1.remote_ammo_exchange_count = 2;
  upstream.info1.remote_health_exchange_count = 3;

  const ActionRequest ammo = make_remote_ammo_exchange(upstream, 1);
  CHECK(ammo.kind == DecisionActionKind::kRemoteAmmoExchange);
  CHECK(ammo.target == 3);
  CHECK(ammo.verify == ActionVerify::kRemoteAmmoCount);
  CHECK(ammo.monotonic);

  const ActionRequest hp = make_remote_hp_exchange(upstream, 1);
  CHECK(hp.kind == DecisionActionKind::kRemoteHpExchange);
  CHECK(hp.target == 4);
  CHECK(hp.verify == ActionVerify::kRemoteHpCount);

  const ActionRequest invalid = make_remote_ammo_exchange(upstream, 2);
  CHECK(invalid.target == 0);
}

void test_revive_factory() {
  const ActionRequest free_revive = make_free_resurrect();
  CHECK(free_revive.kind == DecisionActionKind::kFreeResurrect);
  CHECK(free_revive.target == 1);
  CHECK(free_revive.level);
  CHECK(free_revive.verify == ActionVerify::kAlive);

  const ActionRequest instant = make_instant_resurrect();
  CHECK(instant.kind == DecisionActionKind::kInstantResurrect);
  CHECK(instant.level);
}

// 每个请求只发一次；生效后报告 effective，但不再重发。
void test_sends_once_and_reports() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.remote_ammo_exchange_count = 0;
  const TimePoint t0{};

  const ActionRequest request =
      make_request(DecisionActionKind::kRemoteAmmoExchange, 1, 1, ActionVerify::kRemoteAmmoCount);
  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{100}, &upstream).empty());
  CHECK(!dispatcher.last_report().has_value());

  upstream.info1.remote_ammo_exchange_count = 1;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{200}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
}

// 已达成目标：启动时直接跳过，不下发。
void test_skip_already_achieved() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 150;
  const TimePoint t0{};

  dispatcher.submit(
      make_request(DecisionActionKind::kAmmoExchange, 150, 50, ActionVerify::kAmmoExchangedLocal));
  CHECK(dispatcher.poll(t0, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
}

// 裁判数据无效时暂停；恢复后只发一次。
void test_invalid_upstream_pauses() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.valid = false;
  upstream.info1.ammo_exchanged_local = 0;
  const TimePoint t0{};

  const ActionRequest request =
      make_request(DecisionActionKind::kAmmoExchange, 50, 50, ActionVerify::kAmmoExchangedLocal);
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0, &upstream).empty());

  upstream.valid = true;
  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(first.size() == 1);

  upstream.valid = false;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{500}, &upstream).empty());
}

// 增量变化 -> 新请求，使用新的 request_id。
void test_value_change_starts_new_series() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(
      make_request(DecisionActionKind::kAmmoExchange, 150, 50, ActionVerify::kAmmoExchangedLocal));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 150);

  upstream.info1.ammo_exchanged_local = 150;
  dispatcher.submit(
      make_request(DecisionActionKind::kAmmoExchange, 170, 20, ActionVerify::kAmmoExchangedLocal));
  const auto second = dispatcher.poll(t0 + Duration{200}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 170);
  CHECK(second[0].request_id != first[0].request_id);
}

// 累计目标只增不减。
void test_target_never_decreases() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(
      make_request(DecisionActionKind::kAmmoExchange, 150, 50, ActionVerify::kAmmoExchangedLocal));
  CHECK(dispatcher.poll(t0, &upstream).size() == 1);

  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());
  dispatcher.submit(
      make_request(DecisionActionKind::kAmmoExchange, 120, 20, ActionVerify::kAmmoExchangedLocal));
  const auto second = dispatcher.poll(t0 + Duration{100}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 150);
}

// 电平动作只置位一次；复活后补发 0。
void test_level_action_sets_once_then_clears() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  const ActionRequest request =
      make_request(DecisionActionKind::kFreeResurrect, 1, 1, ActionVerify::kAlive, false, true);
  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{100}, &upstream).empty());

  upstream.self_hp = 40;
  dispatcher.submit(request);
  const auto cleared = dispatcher.poll(t0 + Duration{200}, &upstream);
  CHECK(cleared.size() == 1);
  CHECK(cleared[0].value == 0);
  CHECK(cleared[0].request_id != first[0].request_id);
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
}

// 电平动作在请求消失时补发清零。
void test_level_cleared_when_request_disappears() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  dispatcher.submit(
      make_request(DecisionActionKind::kFreeResurrect, 1, 1, ActionVerify::kAlive, false, true));
  CHECK(dispatcher.poll(t0, &upstream).size() == 1);

  const auto cleared = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(cleared.size() == 1);
  CHECK(cleared[0].value == 0);
}

// 不校验模式（回放 / 无裁判数据）：只发一次。
void test_null_upstream_sends_once() {
  ActionDispatcher dispatcher;
  const TimePoint t0{};
  const ActionRequest request =
      make_request(DecisionActionKind::kAmmoExchange, 50, 50, ActionVerify::kAmmoExchangedLocal);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0, nullptr).size() == 1);
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{500}, nullptr).empty());
}

}  // namespace

int main() {
  test_verify_action();
  test_ammo_factory();
  test_remote_factory();
  test_revive_factory();
  test_sends_once_and_reports();
  test_skip_already_achieved();
  test_invalid_upstream_pauses();
  test_value_change_starts_new_series();
  test_target_never_decreases();
  test_level_action_sets_once_then_clears();
  test_level_cleared_when_request_disappears();
  test_null_upstream_sends_once();
  if (g_failures == 0) {
    std::printf("all action dispatcher tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
