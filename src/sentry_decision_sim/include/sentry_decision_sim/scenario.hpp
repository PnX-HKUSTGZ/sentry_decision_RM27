#pragma once

#include <map>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision_sim {

// 场景取值：set_world 用数值 / 布尔；expect 额外允许字符串（如 tactical_mode: retreat）。
struct ScenarioValue {
  enum class Type { kNumber, kBool, kString };

  Type type = Type::kNumber;
  double number = 0.0;
  bool boolean = false;
  std::string text;
};

// 场景时间轴上的一个事件。
struct ScenarioEvent {
  enum class Kind { kSetWorld, kExpect };

  sentry_decision::Duration at{};
  Kind kind = Kind::kSetWorld;
  // set_world / expect 的字段 -> 取值。
  std::map<std::string, ScenarioValue> args;
};

struct Scenario {
  std::string name;
  // 初始世界（可选）：进程启动时、时间轴之前应用。
  std::map<std::string, ScenarioValue> initial_world;
  // 已按 at 升序排列。
  std::vector<ScenarioEvent> events;
  // 最后一个事件的时刻，供运行方判断场景结束。
  sentry_decision::Duration end{};
};

struct ScenarioLoadResult {
  Scenario scenario;
  std::vector<std::string> errors;

  bool ok() const {
    return errors.empty();
  }
};

// 从 YAML 文件加载场景。解析失败不抛异常，错误集中收集在 errors。
ScenarioLoadResult load_scenario(const std::string& path);

}  // namespace sentry_decision_sim
