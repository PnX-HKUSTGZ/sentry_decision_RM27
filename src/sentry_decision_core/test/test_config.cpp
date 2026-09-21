#include <cstdio>
#include <string>

#include "sentry_decision_core/config.hpp"

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

void test_point_lookup() {
  PolicyConfig config;
  config.points["home"] = Point2D{-5.0, 3.0, 0.25};
  CHECK(config.find_point("home") != nullptr);
  CHECK(config.find_point("home")->x == -5.0);
  CHECK(config.find_point("home")->yaw == 0.25);
  CHECK(config.find_point("missing") == nullptr);
}

void test_number_and_flag_lookup() {
  PolicyConfig config;
  config.numbers["nav.retreat_hp"] = 50.0;
  config.flags["pre_match.allow_cross_undulation"] = true;

  const auto hp = config.number("nav.retreat_hp");
  CHECK(hp.has_value());
  CHECK(hp.value() == 50.0);
  CHECK(!config.number("nav.missing").has_value());

  const auto flag = config.flag("pre_match.allow_cross_undulation");
  CHECK(flag.has_value());
  CHECK(flag.value());
  CHECK(!config.flag("nav.retreat_hp").has_value());
}

}  // namespace

int main() {
  test_point_lookup();
  test_number_and_flag_lookup();
  if (g_failures == 0) {
    std::printf("all config tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
