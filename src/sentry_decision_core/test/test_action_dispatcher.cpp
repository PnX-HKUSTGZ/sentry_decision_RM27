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

ActionRequest make_request(DecisionActionKind kind, int target, int delta, Duration retry,
                           int max_attempts, ActionVerify verify, bool monotonic = true,
                           bool level = false) {
  ActionRequest request;
  request.kind = kind;
  request.target = target;
  request.delta = delta;
  request.retry_interval = retry;
  request.max_attempts = max_attempts;
  request.verify = verify;
  request.monotonic = monotonic;
  request.level = level;
  return request;
}

// 生效判据：兑换看 0x020D 累计值，复活看血量从 0 变正。
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

// 工厂：增量 -> 协议累计目标值；10 发最小单位；1000 上限。
void test_ammo_factory() {
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;

  const ActionRequest ok = make_ammo_exchange(upstream, 50, Duration{600}, 3);
  CHECK(ok.kind == DecisionActionKind::kAmmoExchange);
  CHECK(ok.delta == 50);
  CHECK(ok.target == 150);
  CHECK(ok.verify == ActionVerify::kAmmoExchangedLocal);
  CHECK(ok.monotonic);
  CHECK(ok.retry_interval == Duration{600});
  CHECK(ok.max_attempts == 3);

  // 非 10 的倍数向下取整并发 WARN。
  const ActionRequest rounded = make_ammo_exchange(upstream, 55, Duration{600}, 3);
  CHECK(rounded.delta == 50);
  CHECK(rounded.target == 150);

  // 超过上限时夹到 1000。
  upstream.info1.ammo_exchanged_local = 990;
  const ActionRequest capped = make_ammo_exchange(upstream, 50, Duration{600}, 3);
  CHECK(capped.target == 1000);
}

void test_remote_factory() {
  UpstreamState upstream = make_upstream();
  upstream.info1.remote_ammo_exchange_count = 2;
  upstream.info1.remote_health_exchange_count = 3;

  const ActionRequest ammo = make_remote_ammo_exchange(upstream, 1, Duration{600}, 3);
  CHECK(ammo.kind == DecisionActionKind::kRemoteAmmoExchange);
  CHECK(ammo.target == 3);
  CHECK(ammo.verify == ActionVerify::kRemoteAmmoCount);
  CHECK(ammo.monotonic);

  const ActionRequest hp = make_remote_hp_exchange(upstream, 1, Duration{600}, 3);
  CHECK(hp.kind == DecisionActionKind::kRemoteHpExchange);
  CHECK(hp.target == 4);
  CHECK(hp.verify == ActionVerify::kRemoteHpCount);

  // 规则要求每次只能 +1，一次请求 2 次是非法请求。
  const ActionRequest invalid = make_remote_ammo_exchange(upstream, 2, Duration{600}, 3);
  CHECK(invalid.target == 0);
}

void test_revive_factory() {
  const ActionRequest free_revive = make_free_resurrect(Duration{200}, 0);
  CHECK(free_revive.kind == DecisionActionKind::kFreeResurrect);
  CHECK(free_revive.target == 1);
  CHECK(free_revive.level);
  CHECK(free_revive.verify == ActionVerify::kAlive);
  CHECK(free_revive.max_attempts == 0);

  const ActionRequest instant = make_instant_resurrect(Duration{200}, 3);
  CHECK(instant.kind == DecisionActionKind::kInstantResurrect);
  CHECK(instant.level);
  CHECK(instant.max_attempts == 3);
}

// 生效后不再重发，并产生 effective 报告。
void test_verify_stops_resend() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.remote_ammo_exchange_count = 0;
  const TimePoint t0{};

  const ActionRequest request = make_request(
      DecisionActionKind::kRemoteAmmoExchange, 1, 1, Duration{100}, 3,
      ActionVerify::kRemoteAmmoCount);

  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{99}, &upstream).empty());

  upstream.info1.remote_ammo_exchange_count = 1;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{100}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(report->effective);
  CHECK(report->attempts == 1);
}

// 未生效：按间隔重发同一目标（同一 request_id），达到上限后放弃。
void test_retry_then_give_up() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 0;
  const TimePoint t0{};

  const ActionRequest request = make_request(
      DecisionActionKind::kAmmoExchange, 50, 50, Duration{100}, 3,
      ActionVerify::kAmmoExchangedLocal);

  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 50);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());

  dispatcher.submit(request);
  const auto second = dispatcher.poll(t0 + Duration{100}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 50);
  CHECK(second[0].request_id == first[0].request_id);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{200}, &upstream).size() == 1);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{300}, &upstream).empty());
  const auto report = dispatcher.last_report();
  CHECK(report.has_value());
  CHECK(!report->effective);
  CHECK(report->attempts == 3);

  // 放弃后，只要请求还在就不再重试。
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{1000}, &upstream).empty());
}

// 裁判数据无效时暂停：不开始、不判定、不重发。
void test_invalid_upstream_pauses() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.valid = false;
  upstream.info1.ammo_exchanged_local = 0;
  const TimePoint t0{};

  const ActionRequest request = make_request(
      DecisionActionKind::kAmmoExchange, 50, 50, Duration{100}, 3,
      ActionVerify::kAmmoExchangedLocal);

  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0, &upstream).empty());

  upstream.valid = true;
  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 50);

  upstream.valid = false;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{500}, &upstream).empty());

  upstream.valid = true;
  dispatcher.submit(request);
  CHECK(dispatcher.poll(t0 + Duration{600}, &upstream).size() == 1);
}

// 增量变化 -> 新一段请求，使用新的 request_id。
void test_value_change_starts_new_series() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(make_request(DecisionActionKind::kAmmoExchange, 150, 50, Duration{100}, 3,
                                 ActionVerify::kAmmoExchangedLocal));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 150);

  // 上一段已经生效，累计值变为 150；决策层又请求 20。
  upstream.info1.ammo_exchanged_local = 150;
  dispatcher.submit(make_request(DecisionActionKind::kAmmoExchange, 170, 20, Duration{100}, 3,
                                 ActionVerify::kAmmoExchangedLocal));
  const auto second = dispatcher.poll(t0 + Duration{200}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 170);
  CHECK(second[0].request_id != first[0].request_id);
}

// 累计目标只增不减：新请求 target 更低时，仍使用历史最大值。
void test_target_never_decreases() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.info1.ammo_exchanged_local = 100;
  const TimePoint t0{};

  dispatcher.submit(make_request(DecisionActionKind::kAmmoExchange, 150, 50, Duration{100}, 3,
                                 ActionVerify::kAmmoExchangedLocal));
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 150);

  // 停止提交，条目清除；新请求的 target 回退到 120。
  CHECK(dispatcher.poll(t0 + Duration{50}, &upstream).empty());
  dispatcher.submit(make_request(DecisionActionKind::kAmmoExchange, 120, 20, Duration{100}, 3,
                                 ActionVerify::kAmmoExchangedLocal));
  const auto second = dispatcher.poll(t0 + Duration{100}, &upstream);
  CHECK(second.size() == 1);
  CHECK(second[0].value == 150);
}

// 免费复活：读条期间无限重发 1；复活后补发 0 清零。
void test_free_resurrect_is_level_until_alive() {
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  const ActionRequest request =
      make_request(DecisionActionKind::kFreeResurrect, 1, 1, Duration{100}, 0,
                   ActionVerify::kAlive, false, true);

  dispatcher.submit(request);
  const auto first = dispatcher.poll(t0, &upstream);
  CHECK(first.size() == 1);
  CHECK(first[0].value == 1);

  // 免费复活不限次数：超过普通上限（3）后仍持续置位。
  for (int i = 1; i <= 5; ++i) {
    dispatcher.submit(request);
    const auto again = dispatcher.poll(t0 + Duration{100} * i, &upstream);
    CHECK(again.size() == 1);
    CHECK(again[0].value == 1);
  }

  upstream.self_hp = 40;
  dispatcher.submit(request);
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
  ActionDispatcher dispatcher;
  UpstreamState upstream = make_upstream();
  upstream.self_hp = 0;
  upstream.info1.can_free_resurrect = true;
  const TimePoint t0{};

  dispatcher.submit(make_request(DecisionActionKind::kFreeResurrect, 1, 1, Duration{100}, 0,
                                 ActionVerify::kAlive, false, true));
  CHECK(dispatcher.poll(t0, &upstream).size() == 1);

  const auto cleared = dispatcher.poll(t0 + Duration{50}, &upstream);
  CHECK(cleared.size() == 1);
  CHECK(cleared[0].value == 0);
}

// 不校验模式（回放 / 无裁判数据）：每次由无到有只发一次。
void test_null_upstream_sends_once() {
  ActionDispatcher dispatcher;
  const TimePoint t0{};
  const ActionRequest request = make_request(
      DecisionActionKind::kAmmoExchange, 50, 50, Duration{100}, 3,
      ActionVerify::kAmmoExchangedLocal);

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
  test_verify_stops_resend();
  test_retry_then_give_up();
  test_invalid_upstream_pauses();
  test_value_change_starts_new_series();
  test_target_never_decreases();
  test_free_resurrect_is_level_until_alive();
  test_level_cleared_when_request_disappears();
  test_null_upstream_sends_once();
  if (g_failures == 0) {
    std::printf("all action dispatcher tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
