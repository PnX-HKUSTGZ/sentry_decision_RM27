#include <cstdio>

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

class FakeReferee : public RefereeSource {
 public:
  RefereeState state;
  bool available = true;
  bool referee(RefereeState* out) const override {
    if (!available) {
      return false;
    }
    *out = state;
    return true;
  }
};

class FakeOdometry : public OdometrySource {
 public:
  SelfState state;
  bool available = true;
  bool odometry(SelfState* out) const override {
    if (!available) {
      return false;
    }
    *out = state;
    return true;
  }
};

class FakeNavigation : public NavigationSink {
 public:
  NavState state;
  Point2D last_goal{};
  bool goal_sent = false;
  bool canceled = false;
  void send_goal(const Point2D& goal) override {
    last_goal = goal;
    goal_sent = true;
  }
  void cancel_goal() override {
    canceled = true;
  }
  NavState status() const override {
    return state;
  }
};

void test_fresh_inputs() {
  const TimePoint t0{};
  FakeReferee referee;
  referee.state.stamp = t0;
  referee.state.valid = true;
  referee.state.self_hp = 400;
  FakeOdometry odometry;
  odometry.state.stamp = t0;
  odometry.state.valid = true;
  FakeNavigation navigation;
  navigation.state.stamp = t0;
  navigation.state.valid = true;

  WorldModel model(referee, odometry, navigation);
  const WorldState state = model.snapshot(t0);
  CHECK(state.referee.valid);
  CHECK(state.referee.self_hp == 400);
  CHECK(state.self.valid);
  CHECK(state.nav.valid);
}

void test_timeout_marks_invalid() {
  const TimePoint t0{};
  FakeReferee referee;
  referee.state.stamp = t0;
  referee.state.valid = true;
  FakeOdometry odometry;
  odometry.state.stamp = t0;
  odometry.state.valid = true;
  FakeNavigation navigation;
  navigation.state.stamp = t0;
  navigation.state.valid = true;

  WorldTimeouts timeouts;
  timeouts.referee = Duration{100};
  timeouts.odometry = Duration{50};
  timeouts.navigation = Duration{100};
  WorldModel model(referee, odometry, navigation, timeouts);

  const WorldState fresh_state = model.snapshot(t0);
  CHECK(fresh_state.referee.valid);
  CHECK(fresh_state.self.valid);
  CHECK(fresh_state.nav.valid);

  const WorldState stale_state = model.snapshot(t0 + Duration{101});
  CHECK(!stale_state.referee.valid);
  CHECK(!stale_state.self.valid);
  CHECK(!stale_state.nav.valid);
}

void test_unavailable_source() {
  const TimePoint t0{};
  FakeReferee referee;
  referee.available = false;
  FakeOdometry odometry;
  odometry.available = false;
  FakeNavigation navigation;

  WorldModel model(referee, odometry, navigation);
  const WorldState state = model.snapshot(t0);
  CHECK(!state.referee.valid);
  CHECK(!state.self.valid);
  CHECK(!state.nav.valid);
}

void test_navigation_sink() {
  const TimePoint t0{};
  FakeReferee referee;
  FakeOdometry odometry;
  FakeNavigation navigation;

  WorldModel model(referee, odometry, navigation);
  const Point2D goal{1.5, 2.5, 0.0};
  navigation.send_goal(goal);
  CHECK(navigation.goal_sent);
  CHECK(navigation.last_goal.x == 1.5);
  navigation.cancel_goal();
  CHECK(navigation.canceled);
  (void)model;
}

}  // namespace

int main() {
  test_fresh_inputs();
  test_timeout_marks_invalid();
  test_unavailable_source();
  test_navigation_sink();
  if (g_failures == 0) {
    std::printf("all core world_model tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
