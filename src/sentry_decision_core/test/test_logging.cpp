#include <cstdio>
#include <memory>
#include <string>

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

void test_level_names() {
  CHECK(std::string(sentry_decision::to_string(LogLevel::kDebug)) == "DEBUG");
  CHECK(std::string(sentry_decision::to_string(LogLevel::kAct)) == "ACT");
  CHECK(std::string(sentry_decision::to_string(LogLevel::kError)) == "ERROR");
}

void test_dual_channel() {
  auto full = std::make_shared<MemorySink>();
  auto short_channel = std::make_shared<MemorySink>();
  Logger& logger = Logger::instance();
  logger.clear_sinks();
  logger.add_full_sink(full);
  logger.add_short_sink(short_channel);
  logger.set_full_min_level(LogLevel::kDebug);
  logger.set_short_min_level(LogLevel::kAct);

  SD_LOG_DEBUG("unit", "debug %d", 1);
  SD_LOG_INFO("unit", "info %d", 2);
  SD_LOG_ACT("unit", "act %d", 3);
  SD_LOG_WARN("unit", "warn %d", 4);

  CHECK(full->records().size() == 4);
  CHECK(short_channel->records().size() == 2);
  CHECK(short_channel->records()[0].level == LogLevel::kAct);
  CHECK(short_channel->records()[1].level == LogLevel::kWarn);
  logger.clear_sinks();
}

void test_message_format() {
  auto full = std::make_shared<MemorySink>();
  Logger& logger = Logger::instance();
  logger.clear_sinks();
  logger.add_full_sink(full);
  logger.set_full_min_level(LogLevel::kDebug);
  SD_LOG_ACT("nav", "goal %d -> %d", 3, 7);
  CHECK(full->records().size() == 1);
  CHECK(full->records()[0].tag == "nav");
  CHECK(full->records()[0].message == "goal 3 -> 7");
  logger.clear_sinks();
}

void test_threshold_filter() {
  auto full = std::make_shared<MemorySink>();
  Logger& logger = Logger::instance();
  logger.clear_sinks();
  logger.add_full_sink(full);
  logger.set_full_min_level(LogLevel::kWarn);
  SD_LOG_INFO("unit", "hidden");
  SD_LOG_ERROR("unit", "shown");
  CHECK(full->records().size() == 1);
  CHECK(full->records()[0].message == "shown");
  logger.clear_sinks();
}

}  // namespace

int main() {
  test_level_names();
  test_dual_channel();
  test_message_format();
  test_threshold_filter();
  if (g_failures == 0) {
    std::printf("all core logging tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
