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

class FakeUpstream : public UpstreamSource {
 public:
  UpstreamState state;
  bool available = true;
  bool upstream(UpstreamState* out) const override {
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
  FakeUpstream upstream;
  upstream.state.stamp = t0;
  upstream.state.valid = true;
  upstream.state.self_hp = 400;
  FakeOdometry odometry;
  odometry.state.stamp = t0;
  odometry.state.valid = true;
  FakeNavigation navigation;
  navigation.state.stamp = t0;
  navigation.state.valid = true;

  WorldModel model(upstream, odometry, navigation,
                   WorldTimeouts{Duration{100}, Duration{50}, Duration{100}});
  const WorldState state = model.snapshot(t0);
  CHECK(state.upstream.valid);
  CHECK(state.upstream.self_hp == 400);
  CHECK(state.self.valid);
  CHECK(state.nav.valid);
}

void test_timeout_marks_invalid() {
  const TimePoint t0{};
  FakeUpstream upstream;
  upstream.state.stamp = t0;
  upstream.state.valid = true;
  FakeOdometry odometry;
  odometry.state.stamp = t0;
  odometry.state.valid = true;
  FakeNavigation navigation;
  navigation.state.stamp = t0;
  navigation.state.valid = true;

  WorldTimeouts timeouts;
  timeouts.upstream = Duration{100};
  timeouts.odometry = Duration{50};
  timeouts.navigation = Duration{100};
  WorldModel model(upstream, odometry, navigation, timeouts);

  const WorldState fresh_state = model.snapshot(t0);
  CHECK(fresh_state.upstream.valid);
  CHECK(fresh_state.self.valid);
  CHECK(fresh_state.nav.valid);

  const WorldState stale_state = model.snapshot(t0 + Duration{101});
  CHECK(!stale_state.upstream.valid);
  CHECK(!stale_state.self.valid);
  CHECK(!stale_state.nav.valid);
}

void test_unavailable_source() {
  const TimePoint t0{};
  FakeUpstream upstream;
  upstream.available = false;
  FakeOdometry odometry;
  odometry.available = false;
  FakeNavigation navigation;

  WorldModel model(upstream, odometry, navigation,
                   WorldTimeouts{Duration{100}, Duration{50}, Duration{100}});
  const WorldState state = model.snapshot(t0);
  CHECK(!state.upstream.valid);
  CHECK(!state.self.valid);
  CHECK(!state.nav.valid);
}

WorldModel make_owner_model(FakeUpstream* upstream, FakeOdometry* odometry,
                            FakeNavigation* navigation) {
  const TimePoint t0{};
  upstream->state.stamp = t0;
  upstream->state.valid = true;
  upstream->state.game_status = GameStatus::kRunning;
  upstream->state.our_outpost_hp = 1000;
  upstream->state.our_outpost_control = OutpostControl::kOwnFull;
  upstream->state.enemy_outpost_hp = 1000;
  upstream->state.enemy_outpost_control = OutpostControl::kNotOwned;
  odometry->state.stamp = t0;
  odometry->state.valid = true;
  navigation->state.stamp = t0;
  navigation->state.valid = true;
  return WorldModel(*upstream, *odometry, *navigation,
                    WorldTimeouts{Duration{10000}, Duration{10000}, Duration{10000}});
}

void test_outpost_owner_rebuild_toggles() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;
  WorldModel model = make_owner_model(&upstream, &odometry, &navigation);

  const WorldState initial = model.snapshot(t0);
  CHECK(initial.upstream.our_outpost_owner == OutpostOwner::kOwn);
  CHECK(initial.upstream.enemy_outpost_owner == OutpostOwner::kEnemy);

  // 己方完全控制被击毁 -> 己方临时控制，HP 0；归属不变。
  upstream.state.stamp = t0 + Duration{100};
  upstream.state.our_outpost_hp = 0;
  upstream.state.our_outpost_control = OutpostControl::kOwnTemporary;
  const WorldState broken = model.snapshot(t0 + Duration{100});
  CHECK(broken.upstream.our_outpost_owner == OutpostOwner::kOwn);

  // HP 上升 = 重建；控制方为敌方，归属反转到敌方。
  upstream.state.stamp = t0 + Duration{200};
  upstream.state.our_outpost_hp = 500;
  upstream.state.our_outpost_control = OutpostControl::kNotOwned;
  const WorldState rebuilt = model.snapshot(t0 + Duration{200});
  CHECK(rebuilt.upstream.our_outpost_owner == OutpostOwner::kEnemy);

  const WorldState settled = model.snapshot(t0 + Duration{1300});
  CHECK(settled.upstream.our_outpost_owner == OutpostOwner::kEnemy);
}

void test_outpost_owner_temporary_lag_keeps_toggle() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;
  WorldModel model = make_owner_model(&upstream, &odometry, &navigation);

  upstream.state.stamp = t0 + Duration{100};
  upstream.state.enemy_outpost_hp = 0;
  upstream.state.enemy_outpost_control = OutpostControl::kOwnTemporary;
  const WorldState broken = model.snapshot(t0 + Duration{100});
  CHECK(broken.upstream.enemy_outpost_owner == OutpostOwner::kEnemy);

  // 路径 1 击毁：HP 0 -> 500；控制位暂时仍是「我方临时」，不能据此改成己方。
  upstream.state.stamp = t0 + Duration{200};
  upstream.state.enemy_outpost_hp = 500;
  const WorldState rebuilt = model.snapshot(t0 + Duration{200});
  CHECK(rebuilt.upstream.enemy_outpost_owner == OutpostOwner::kOwn);

  const WorldState settled = model.snapshot(t0 + Duration{1300});
  CHECK(settled.upstream.enemy_outpost_owner == OutpostOwner::kOwn);
}

void test_outpost_owner_full_control_corrects() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;
  WorldModel model = make_owner_model(&upstream, &odometry, &navigation);

  upstream.state.stamp = t0 + Duration{100};
  upstream.state.enemy_outpost_control = OutpostControl::kOwnFull;
  const WorldState state = model.snapshot(t0 + Duration{100});
  CHECK(state.upstream.enemy_outpost_owner == OutpostOwner::kOwn);
}

void test_outpost_owner_resets_outside_match() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;
  WorldModel model = make_owner_model(&upstream, &odometry, &navigation);

  upstream.state.stamp = t0 + Duration{100};
  upstream.state.our_outpost_hp = 0;
  upstream.state.our_outpost_control = OutpostControl::kOwnTemporary;
  const WorldState broken = model.snapshot(t0 + Duration{100});
  CHECK(broken.upstream.our_outpost_owner == OutpostOwner::kOwn);

  upstream.state.stamp = t0 + Duration{200};
  upstream.state.our_outpost_hp = 500;
  upstream.state.our_outpost_control = OutpostControl::kNotOwned;
  const WorldState changed = model.snapshot(t0 + Duration{200});
  CHECK(changed.upstream.our_outpost_owner == OutpostOwner::kEnemy);

  upstream.state.stamp = t0 + Duration{300};
  upstream.state.game_status = GameStatus::kPreparation;
  const WorldState reset = model.snapshot(t0 + Duration{300});
  CHECK(reset.upstream.our_outpost_owner == OutpostOwner::kOwn);
  CHECK(reset.upstream.enemy_outpost_owner == OutpostOwner::kEnemy);
}

void test_outpost_hp_trend_and_attack_flag() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;
  WorldModel model = make_owner_model(&upstream, &odometry, &navigation);

  const WorldState initial = model.snapshot(t0);
  CHECK(initial.our_outpost_trend.hp_delta_1s == 0);
  CHECK(!initial.our_outpost_trend.under_attack);

  upstream.state.stamp = t0 + Duration{100};
  upstream.state.our_outpost_hp = 700;
  const WorldState hit = model.snapshot(t0 + Duration{100});
  CHECK(hit.our_outpost_trend.under_attack);

  upstream.state.stamp = t0 + Duration{1100};
  upstream.state.our_outpost_hp = 600;
  const WorldState trend = model.snapshot(t0 + Duration{1100});
  CHECK(trend.our_outpost_trend.hp_delta_1s == -400);

  upstream.state.stamp = t0 + Duration{2700};
  upstream.state.our_outpost_hp = 600;
  const WorldState settled = model.snapshot(t0 + Duration{2700});
  CHECK(!settled.our_outpost_trend.under_attack);

  upstream.state.stamp = t0 + Duration{2800};
  upstream.state.our_outpost_hp = 600;
  upstream.state.game_status = GameStatus::kPreparation;
  const WorldState reset = model.snapshot(t0 + Duration{2800});
  CHECK(reset.our_outpost_trend.hp_delta_1s == 0);
  CHECK(!reset.our_outpost_trend.under_attack);
}

void test_navigation_sink() {
  const TimePoint t0{};
  FakeUpstream upstream;
  FakeOdometry odometry;
  FakeNavigation navigation;

  WorldModel model(upstream, odometry, navigation,
                   WorldTimeouts{Duration{100}, Duration{50}, Duration{100}});
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
  test_outpost_owner_rebuild_toggles();
  test_outpost_owner_temporary_lag_keeps_toggle();
  test_outpost_owner_full_control_corrects();
  test_outpost_owner_resets_outside_match();
  test_outpost_hp_trend_and_attack_flag();
  test_navigation_sink();
  if (g_failures == 0) {
    std::printf("all core world_model tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
