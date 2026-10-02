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

DecisionAction make_action(DecisionActionKind kind, ActionMode mode, int value,
                           Duration interval = Duration{0}) {
  DecisionAction action;
  action.kind = kind;
  action.mode = mode;
  action.value = value;
  action.interval = interval;
  return action;
}

void test_one_shot_sends_once() {
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kAmmoExchange, ActionMode::kOneShot, 50);

  dispatcher.submit(action);
  auto outgoing = dispatcher.poll(t0);
  CHECK(outgoing.size() == 1);
  CHECK(outgoing[0].value == 50);
  CHECK(outgoing[0].request_id != 0);

  // 持续提交同一请求：不重发。
  dispatcher.submit(action);
  outgoing = dispatcher.poll(t0 + Duration{50});
  CHECK(outgoing.empty());

  // 收到 ack 后，同值仍不重发。
  ActionAck ack;
  ack.request_id = 1;
  ack.accepted = true;
  dispatcher.on_ack(ack);
  dispatcher.submit(action);
  outgoing = dispatcher.poll(t0 + Duration{100});
  CHECK(outgoing.empty());

  // 值变化 -> 新请求。
  const DecisionAction changed =
      make_action(DecisionActionKind::kAmmoExchange, ActionMode::kOneShot, 100);
  dispatcher.submit(changed);
  outgoing = dispatcher.poll(t0 + Duration{150});
  CHECK(outgoing.size() == 1);
  CHECK(outgoing[0].value == 100);
  CHECK(outgoing[0].request_id == 2);
}

// 停止提交后再提交同值，视为新请求（由无到有）。
void test_one_shot_rearms_after_gap() {
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kFreeResurrect, ActionMode::kOneShot, 0);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);
  // 停止提交：条目被清理。
  CHECK(dispatcher.poll(t0 + Duration{50}).empty());
  // 再次提交同值 -> 重新发送。
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{100}).size() == 1);
}

// 人工重复触发：rearm 清除 one-shot 记忆后，同值可再次发送。
void test_rearm_allows_same_value_again() {
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kAmmoExchange, ActionMode::kOneShot, 50);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{50}).empty());

  dispatcher.rearm(DecisionActionKind::kAmmoExchange);
  dispatcher.submit(action);
  const auto outgoing = dispatcher.poll(t0 + Duration{100});
  CHECK(outgoing.size() == 1);
  CHECK(outgoing[0].value == 50);
}

void test_polled_resends_at_interval() {
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kRemoteAmmoExchange, ActionMode::kPolled, 1, Duration{100});

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{50}).empty());

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{100}).size() == 1);

  // 停止提交 -> 停止重发。
  CHECK(dispatcher.poll(t0 + Duration{200}).empty());
}

void test_polled_zero_interval_every_tick() {
  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kRemoteHpExchange, ActionMode::kPolled, 1, Duration{0});
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);
}

void test_one_shot_timeout_warns() {
  auto sink = std::make_shared<MemorySink>();
  Logger::instance().clear_sinks();
  Logger::instance().add_full_sink(sink);
  Logger::instance().set_full_min_level(LogLevel::kWarn);

  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{100}});
  const TimePoint t0{};
  const DecisionAction action =
      make_action(DecisionActionKind::kHpExchange, ActionMode::kOneShot, 30);
  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0).size() == 1);

  dispatcher.submit(action);
  CHECK(dispatcher.poll(t0 + Duration{150}).empty());

  bool warned = false;
  for (const auto& record : sink->records()) {
    if (record.level == LogLevel::kWarn && record.message.find("超时未确认") != std::string::npos) {
      warned = true;
    }
  }
  CHECK(warned);

  Logger::instance().clear_sinks();
}

void test_unknown_ack_warns() {
  auto sink = std::make_shared<MemorySink>();
  Logger::instance().clear_sinks();
  Logger::instance().add_full_sink(sink);
  Logger::instance().set_full_min_level(LogLevel::kWarn);

  ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
  ActionAck ack;
  ack.request_id = 999;
  dispatcher.on_ack(ack);

  bool warned = false;
  for (const auto& record : sink->records()) {
    if (record.message.find("未知 request_id") != std::string::npos) {
      warned = true;
    }
  }
  CHECK(warned);
  Logger::instance().clear_sinks();
}

// 资源请求 -> 动作类型映射：远程 / 立即复活分别落到对应 kind，立即复活优先。
void test_submit_resource_requests_maps_kinds() {
  const TimePoint t0{};
  {
    ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
    ResourceRequest request;
    request.remote_hp = 2;
    submit_resource_requests(dispatcher, request);
    const auto out = dispatcher.poll(t0);
    CHECK(out.size() == 1);
    CHECK(out[0].kind == DecisionActionKind::kRemoteHpExchange);
    CHECK(out[0].value == 2);
  }
  {
    ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
    ResourceRequest request;
    request.remote_ammo = 1;
    submit_resource_requests(dispatcher, request);
    const auto out = dispatcher.poll(t0);
    CHECK(out.size() == 1);
    CHECK(out[0].kind == DecisionActionKind::kRemoteAmmoExchange);
    CHECK(out[0].value == 1);
  }
  {
    ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
    ResourceRequest request;
    request.instant_revive = true;
    request.revive = true;  // 互斥：立即复活优先
    submit_resource_requests(dispatcher, request);
    const auto out = dispatcher.poll(t0);
    CHECK(out.size() == 1);
    CHECK(out[0].kind == DecisionActionKind::kInstantResurrect);
  }
  {
    ActionDispatcher dispatcher(ActionDispatcherConfig{Duration{500}});
    ResourceRequest request;
    request.ammo = 50;
    request.hp = 30;  // 人工注入的本地血量兑换仍可下发（规则上仅用于调试）
    submit_resource_requests(dispatcher, request);
    CHECK(dispatcher.poll(t0).size() == 2);
  }
}

}  // namespace

int main() {
  test_submit_resource_requests_maps_kinds();
  test_one_shot_sends_once();
  test_one_shot_rearms_after_gap();
  test_rearm_allows_same_value_again();
  test_polled_resends_at_interval();
  test_polled_zero_interval_every_tick();
  test_one_shot_timeout_warns();
  test_unknown_ack_warns();
  if (g_failures == 0) {
    std::printf("all action dispatcher tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
