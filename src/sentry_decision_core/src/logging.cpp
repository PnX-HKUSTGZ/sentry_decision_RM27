#include "sentry_decision_core/logging.hpp"

#include <utility>

namespace sentry_decision {
namespace {

const char* level_name(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kAct:
      return "ACT";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
  }
  return "?";
}

const char* level_color(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "\033[90m";
    case LogLevel::kInfo:
      return "\033[0m";
    case LogLevel::kAct:
      return "\033[36m";
    case LogLevel::kWarn:
      return "\033[33m";
    case LogLevel::kError:
      return "\033[31m";
  }
  return "\033[0m";
}

bool at_least(LogLevel level, LogLevel threshold) {
  return static_cast<int>(level) >= static_cast<int>(threshold);
}

std::string format_message(const char* format, std::va_list args) {
  if (format == nullptr) {
    return {};
  }
  std::va_list copy;
  va_copy(copy, args);
  const int needed = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  if (needed < 0) {
    return std::string(format);
  }
  std::string buffer(static_cast<size_t>(needed) + 1, 0);
  std::vsnprintf(buffer.data(), buffer.size(), format, args);
  buffer.resize(static_cast<size_t>(needed));
  return buffer;
}

}  // namespace

const char* to_string(LogLevel level) {
  return level_name(level);
}

ConsoleSink::ConsoleSink(bool colored) : colored_(colored) {}

void ConsoleSink::write(const LogRecord& record) {
  std::FILE* out = at_least(record.level, LogLevel::kWarn) ? stderr : stdout;
  if (colored_) {
    std::fprintf(out, "%s[%s] [%s] %s\033[0m\n", level_color(record.level),
                 level_name(record.level), record.tag.c_str(), record.message.c_str());
  } else {
    std::fprintf(out, "[%s] [%s] %s\n", level_name(record.level), record.tag.c_str(),
                 record.message.c_str());
  }
  std::fflush(out);
}

FileSink::FileSink(const std::string& path) : file_(std::fopen(path.c_str(), "a")) {}

FileSink::~FileSink() {
  if (file_ != nullptr) {
    std::fclose(file_);
  }
}

void FileSink::write(const LogRecord& record) {
  if (file_ == nullptr) {
    return;
  }
  std::fprintf(file_, "[%s] [%s] %s\n", level_name(record.level), record.tag.c_str(),
               record.message.c_str());
  std::fflush(file_);
}

void MemorySink::write(const LogRecord& record) {
  records_.push_back(record);
}

const std::vector<LogRecord>& MemorySink::records() const {
  return records_;
}

void MemorySink::clear() {
  records_.clear();
}

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::add_full_sink(std::shared_ptr<LogSink> sink) {
  std::lock_guard<std::mutex> lock(mutex_);
  full_sinks_.push_back(std::move(sink));
}

void Logger::add_short_sink(std::shared_ptr<LogSink> sink) {
  std::lock_guard<std::mutex> lock(mutex_);
  short_sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() {
  std::lock_guard<std::mutex> lock(mutex_);
  full_sinks_.clear();
  short_sinks_.clear();
}

void Logger::set_full_min_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  full_min_level_ = level;
}

void Logger::set_short_min_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  short_min_level_ = level;
}

LogLevel Logger::full_min_level() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_min_level_;
}

LogLevel Logger::short_min_level() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return short_min_level_;
}

void Logger::log(LogLevel level, const char* tag, const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  log_v(level, tag, format, args);
  va_end(args);
}

void Logger::log_v(LogLevel level, const char* tag, const char* format, std::va_list args) {
  LogRecord record;
  record.level = level;
  record.tag = (tag == nullptr) ? "" : tag;
  record.message = format_message(format, args);

  std::vector<std::shared_ptr<LogSink>> full;
  std::vector<std::shared_ptr<LogSink>> short_channel;
  LogLevel full_min = LogLevel::kDebug;
  LogLevel short_min = LogLevel::kAct;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    full = full_sinks_;
    short_channel = short_sinks_;
    full_min = full_min_level_;
    short_min = short_min_level_;
  }
  if (at_least(level, full_min)) {
    for (const auto& sink : full) {
      sink->write(record);
    }
  }
  if (at_least(level, short_min)) {
    for (const auto& sink : short_channel) {
      sink->write(record);
    }
  }
}

}  // namespace sentry_decision
