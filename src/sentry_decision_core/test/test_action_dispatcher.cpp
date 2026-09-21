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
  ActionDispatcher dispatcher;
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
  ActionDispatcher dispatcher;
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

void test_polled_resends_at_interval() {
  ActionDispatcher dispatcher;
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
  ActionDispatcher dispatcher;
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

  ActionDispatcher dispatcher;
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

}  // namespace

int main() {
  test_one_shot_sends_once();
  test_one_shot_rearms_after_gap();
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
