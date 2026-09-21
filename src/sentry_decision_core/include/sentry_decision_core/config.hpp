#pragma once

#include <map>
#include <optional>
#include <string>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 决策配置（纯数据，不依赖 YAML）。
//
// bringup 在启动时把 profiles / maps / policies 三个 YAML 解析成该结构并校验，
// 通过 DecisionContext.config 提供给行为树节点（只读）。
//
// 数值与开关以「点分命名」的扁平 key 保存（如 nav.retreat_hp），
// 这样 XML 只引用 key，改阈值无需改代码 / XML；点位用命名点（如 home）。
struct PolicyConfig {
  std::string map_profile;
  std::string strategy_profile;
  std::string map_frame = "map";
  std::map<std::string, Point2D> points;
  std::map<std::string, double> numbers;
  std::map<std::string, bool> flags;

  const Point2D* find_point(const std::string& name) const {
    const auto it = points.find(name);
    return it == points.end() ? nullptr : &it->second;
  }

  std::optional<double> number(const std::string& key) const {
    const auto it = numbers.find(key);
    if (it == numbers.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  std::optional<bool> flag(const std::string& key) const {
    const auto it = flags.find(key);
    if (it == flags.end()) {
      return std::nullopt;
    }
    return it->second;
  }
};

}  // namespace sentry_decision
