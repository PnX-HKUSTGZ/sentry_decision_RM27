#include <cstdio>
#include <string>

#include "sentry_decision_sim/match_stage.hpp"

using sentry_decision_sim::MatchStage;
using sentry_decision_sim::MatchStageController;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void test_forward_only() {
  MatchStageController controller;
  std::string error;
  CHECK(!controller.active());
  CHECK(controller.stage() == MatchStage::kNotStarted);

  CHECK(controller.set(1, &error));
  CHECK(controller.active());
  CHECK(controller.stage() == MatchStage::kPreparation);
  CHECK(controller.remaining_seconds() == 0);
  CHECK(!controller.counting());

  error.clear();
  CHECK(!controller.set(1, &error));  // 当前阶段拒绝
  CHECK(!error.empty());

  error.clear();
  CHECK(controller.set(3, &error));  // 允许快进
  CHECK(controller.stage() == MatchStage::kCountdown);
  CHECK(controller.remaining_seconds() == 5);
  CHECK(controller.counting());

  error.clear();
  CHECK(!controller.set(2, &error));  // 不可回退
  CHECK(!error.empty());
  CHECK(controller.stage() == MatchStage::kCountdown);

  error.clear();
  CHECK(!controller.set(9, &error));  // 非法取值
  CHECK(controller.stage() == MatchStage::kCountdown);
}

void test_auto_advance() {
  MatchStageController controller;
  std::string error;
  CHECK(controller.set(2, &error));  // 15s 自检
  CHECK(controller.remaining_seconds() == 15);
  for (int i = 0; i < 15; ++i) {
    controller.tick_second();
  }
  CHECK(controller.stage() == MatchStage::kCountdown);
  CHECK(controller.remaining_seconds() == 5);
  for (int i = 0; i < 5; ++i) {
    controller.tick_second();
  }
  CHECK(controller.stage() == MatchStage::kRunning);
  CHECK(controller.remaining_seconds() == 420);
  for (int i = 0; i < 420; ++i) {
    controller.tick_second();
  }
  CHECK(controller.remaining_seconds() == 0);
  CHECK(!controller.counting());
}

void test_reset() {
  MatchStageController controller;
  std::string error;
  CHECK(controller.set(4, &error));
  CHECK(controller.set(0, &error));  // 重置始终允许
  CHECK(controller.stage() == MatchStage::kNotStarted);
  CHECK(controller.remaining_seconds() == 0);
  CHECK(!controller.counting());
  CHECK(controller.active());
  CHECK(controller.set(1, &error));  // 重置后可再次前进
}

}  // namespace

int main() {
  test_forward_only();
  test_auto_advance();
  test_reset();

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_match_stage passed\n");
  return 0;
}
