#include <cstdint>
#include <cstdio>
#include <vector>

#include "sentry_decision_core/replay.hpp"
#include "sentry_decision_core/world_model.hpp"

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

ReplayData make_referee_series() {
  ReplayData data;
  RefereeState first;
  first.valid = true;
  first.self_hp = 400;
  data.referee.push_back(ReplayRecord<RefereeState>{Duration{0}, first});

  RefereeState second;
  second.valid = true;
  second.self_hp = 50;
  data.referee.push_back(ReplayRecord<RefereeState>{Duration{200}, second});

  SelfState self;
  self.valid = true;
  data.odometry.push_back(ReplayRecord<SelfState>{Duration{0}, self});

  NavState nav;
  nav.valid = true;
  data.navigation.push_back(ReplayRecord<NavState>{Duration{0}, nav});
  return data;
}

void test_latest_by_time() {
  ReplaySource replay(make_referee_series());
  CHECK(replay.tick() == 0);
  CHECK(!replay.finished());

  RefereeState state;
  CHECK(replay.referee(&state));  // now = 0 时应取到 at = 0 的记录
  CHECK(state.self_hp == 400);

  replay.step(Duration{50});
  CHECK(replay.tick() == 1);
  CHECK(replay.referee(&state));
  CHECK(state.self_hp == 400);

  replay.step(Duration{150});  // now = 200
  CHECK(replay.referee(&state));
  CHECK(state.self_hp == 50);
}

void test_epoch_stamp_and_freshness() {
  const TimePoint epoch = TimePoint{} + Duration{1000};
  ReplayData data;
  RefereeState referee;
  referee.valid = true;
  referee.self_hp = 400;
  data.referee.push_back(ReplayRecord<RefereeState>{Duration{0}, referee});
  SelfState self;
  self.valid = true;
  data.odometry.push_back(ReplayRecord<SelfState>{Duration{0}, self});
  NavState nav;
  nav.valid = true;
  data.navigation.push_back(ReplayRecord<NavState>{Duration{0}, nav});

  ReplaySource replay(data, epoch);
  WorldModel model(replay, replay, replay);
  CHECK(replay.stamp() == epoch);

  const WorldState fresh = model.snapshot(replay.stamp());
  CHECK(fresh.referee.valid);
  CHECK(fresh.self.valid);
  CHECK(fresh.nav.valid);

  replay.step(Duration{600});  // 超过默认 500ms 有效期
  const WorldState stale = model.snapshot(replay.stamp());
  CHECK(!stale.referee.valid);
  CHECK(!stale.nav.valid);
}

void test_finished_and_empty() {
  ReplaySource replay(make_referee_series());
  CHECK(!replay.finished());
  replay.step(Duration{200});
  CHECK(replay.finished());

  ReplaySource empty{ReplayData{}};
  RefereeState state;
  SelfState self;
  CHECK(!empty.referee(&state));
  CHECK(!empty.odometry(&self));
  CHECK(!empty.status().valid);
}

std::vector<int> run_sequence(const ReplayData& data) {
  ReplaySource replay(data);
  WorldModel model(replay, replay, replay);
  std::vector<int> result;
  for (int i = 0; i < 5; ++i) {
    replay.step(Duration{100});
    result.push_back(model.snapshot(replay.stamp()).referee.self_hp);
  }
  return result;
}

void test_determinism() {
  const ReplayData data = make_referee_series();
  const std::vector<int> first = run_sequence(data);
  const std::vector<int> second = run_sequence(data);
  CHECK(first == second);
  CHECK((first == std::vector<int>{400, 50, 50, 50, 50}));
}

void test_intervention_channel() {
  ReplayData data = make_referee_series();
  InterventionCommand first;
  first.kind = InterventionCommand::Kind::kModuleSwitch;
  first.module = "nav";
  first.enabled = false;
  data.interventions.push_back({Duration{0}, first});

  InterventionCommand second;
  second.kind = InterventionCommand::Kind::kClearWorld;
  data.interventions.push_back({Duration{200}, second});

  ReplaySource replay(data);
  // at = 0 的事件在首次调用即返回，且不重复。
  CHECK(replay.interventions().size() == 1);
  CHECK(replay.interventions().empty());
  replay.step(Duration{100});
  CHECK(replay.interventions().empty());
  replay.step(Duration{100});  // now = 200
  const std::vector<InterventionCommand> due = replay.interventions();
  CHECK(due.size() == 1);
  CHECK(due[0].kind == InterventionCommand::Kind::kClearWorld);
}

void test_navigation_sink_counters() {
  ReplaySource replay(make_referee_series());
  replay.send_goal(Point2D{1.0, 2.0, 0.0});
  replay.send_goal(Point2D{3.0, 4.0, 0.0});
  replay.cancel_goal();
  CHECK(replay.sent_goals() == 2);
  CHECK(replay.canceled_goals() == 1);
  CHECK(replay.last_goal().x == 3.0);
  CHECK(replay.last_goal().y == 4.0);
}

}  // namespace

int main() {
  test_latest_by_time();
  test_epoch_stamp_and_freshness();
  test_finished_and_empty();
  test_determinism();
  test_intervention_channel();
  test_navigation_sink_counters();
  if (g_failures == 0) {
    std::printf("all core replay tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
