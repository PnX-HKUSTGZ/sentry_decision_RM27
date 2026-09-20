#pragma once

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sentry_decision {

// 日志级别。ACT 表示重要决策行为，会进入简短日志与 stdout。
enum class LogLevel : int { kDebug = 0, kInfo = 1, kAct = 2, kWarn = 3, kError = 4 };

const char* to_string(LogLevel level);

struct LogRecord {
  LogLevel level = LogLevel::kInfo;
  std::string tag;
  std::string message;
};

// 日志输出端；完整日志与简短日志各自可以挂多个 sink。
class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void write(const LogRecord& record) = 0;
};

// 简洁单行输出；WARN 及以上写 stderr，其余写 stdout。
class ConsoleSink : public LogSink {
 public:
  explicit ConsoleSink(bool colored = true);
  void write(const LogRecord& record) override;

 private:
  bool colored_;
};

// 追加写入文件；打开失败时静默丢弃。
class FileSink : public LogSink {
 public:
  explicit FileSink(const std::string& path);
  ~FileSink() override;
  FileSink(const FileSink&) = delete;
  FileSink& operator=(const FileSink&) = delete;
  void write(const LogRecord& record) override;

 private:
  std::FILE* file_ = nullptr;
};

// 内存 sink：用于测试与单测断言。
class MemorySink : public LogSink {
 public:
  void write(const LogRecord& record) override;
  const std::vector<LogRecord>& records() const;
  void clear();

 private:
  std::vector<LogRecord> records_;
};

// 全局日志器，是唯一允许的全局基础设施；决策状态仍必须显式传递。
// 完整日志默认 >= DEBUG，简短日志默认 >= ACT。
//
// 当前使用 printf 风格格式化（零依赖、可由编译器做格式检查）；
// 后续如需 fmt / std::format 风格再评估替换。
class Logger {
 public:
  static Logger& instance();

  void add_full_sink(std::shared_ptr<LogSink> sink);
  void add_short_sink(std::shared_ptr<LogSink> sink);
  void clear_sinks();

  void set_full_min_level(LogLevel level);
  void set_short_min_level(LogLevel level);
  LogLevel full_min_level() const;
  LogLevel short_min_level() const;

  void log(LogLevel level, const char* tag, const char* format, ...)
#ifdef __GNUC__
      __attribute__((format(printf, 4, 5)))
#endif
      ;

 private:
  Logger() = default;

  void log_v(LogLevel level, const char* tag, const char* format, std::va_list args);

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<LogSink>> full_sinks_;
  std::vector<std::shared_ptr<LogSink>> short_sinks_;
  LogLevel full_min_level_ = LogLevel::kDebug;
  LogLevel short_min_level_ = LogLevel::kAct;
};

}  // namespace sentry_decision

#define SD_LOG_DEBUG(tag, ...) \
  ::sentry_decision::Logger::instance().log(::sentry_decision::LogLevel::kDebug, tag, __VA_ARGS__)
#define SD_LOG_INFO(tag, ...) \
  ::sentry_decision::Logger::instance().log(::sentry_decision::LogLevel::kInfo, tag, __VA_ARGS__)
#define SD_LOG_ACT(tag, ...) \
  ::sentry_decision::Logger::instance().log(::sentry_decision::LogLevel::kAct, tag, __VA_ARGS__)
#define SD_LOG_WARN(tag, ...) \
  ::sentry_decision::Logger::instance().log(::sentry_decision::LogLevel::kWarn, tag, __VA_ARGS__)
#define SD_LOG_ERROR(tag, ...) \
  ::sentry_decision::Logger::instance().log(::sentry_decision::LogLevel::kError, tag, __VA_ARGS__)
