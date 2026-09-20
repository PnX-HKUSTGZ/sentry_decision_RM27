#include <cstdio>

#include "sentry_decision_sim/referee_simulator.hpp"

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

void test_invalid_before_update() {
  RefereeSimulator sim;
  RefereeState out;
  CHECK(!sim.referee(&out));
}

void test_schedule_fires_once() {
  const TimePoint t0{};
  RefereeSimulator sim;
  sim.mutable_state().self_hp = 400;
  int calls = 0;
  sim.schedule(Duration{100}, [&calls](RefereeState& state) {
    state.self_hp = 50;
    ++calls;
  });

  RefereeState out;
  sim.update(t0);
  CHECK(sim.referee(&out));
  CHECK(out.self_hp == 400);
  CHECK(out.stamp == t0);

  sim.update(t0 + Duration{50});
  CHECK(sim.referee(&out));
  CHECK(out.self_hp == 400);

  sim.update(t0 + Duration{100});
  CHECK(sim.referee(&out));
  CHECK(out.self_hp == 50);
  CHECK(calls == 1);

  sim.update(t0 + Duration{200});
  CHECK(sim.referee(&out));
  CHECK(out.self_hp == 50);
  CHECK(calls == 1);
  CHECK(out.stamp == t0 + Duration{200});
}

}  // namespace

int main() {
  test_invalid_before_update();
  test_schedule_fires_once();
  if (g_failures == 0) {
    std::printf("all sim referee_simulator tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
