#include "sentry_decision_sim/scenario.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <string>

namespace sentry_decision_sim {
namespace {

std::string to_lower(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

bool parse_value(const YAML::Node& node, ScenarioValue* out, std::string* error) {
  if (!node.IsScalar()) {
    *error = "仅支持标量取值";
    return false;
  }
  const std::string text = node.Scalar();
  const std::string lower = to_lower(text);
  if (lower == "true" || lower == "false" || lower == "yes" || lower == "no" || lower == "on" ||
      lower == "off") {
    out->type = ScenarioValue::Type::kBool;
    out->boolean = (lower == "true" || lower == "yes" || lower == "on");
    return true;
  }
  try {
    out->number = std::stod(text);
    out->type = ScenarioValue::Type::kNumber;
    return true;
  } catch (const std::exception&) {
    out->type = ScenarioValue::Type::kString;
    out->text = text;
    return true;
  }
}

bool parse_args(const YAML::Node& node, const std::string& context,
                std::map<std::string, ScenarioValue>* out, std::vector<std::string>* errors) {
  if (!node.IsMap()) {
    errors->push_back(context + " 必须是键值映射");
    return false;
  }
  bool ok = true;
  for (const auto& item : node) {
    const std::string field = item.first.as<std::string>();
    ScenarioValue value;
    std::string error;
    if (!parse_value(item.second, &value, &error)) {
      errors->push_back(context + "." + field + ": " + error);
      ok = false;
      continue;
    }
    (*out)[field] = std::move(value);
  }
  return ok;
}

}  // namespace

ScenarioLoadResult load_scenario(const std::string& path) {
  ScenarioLoadResult result;
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const std::exception& ex) {
    result.errors.push_back(std::string("无法加载场景文件: ") + ex.what());
    return result;
  }

  if (!root.IsMap()) {
    result.errors.push_back("场景根节点必须是映射");
    return result;
  }

  if (root["name"]) {
    result.scenario.name = root["name"].as<std::string>();
  }
  if (root["world"]) {
    parse_args(root["world"], "world", &result.scenario.initial_world, &result.errors);
  }
  if (!root["timeline"]) {
    result.errors.push_back("缺少 timeline");
    return result;
  }
  if (!root["timeline"].IsSequence()) {
    result.errors.push_back("timeline 必须是序列");
    return result;
  }

  std::size_t index = 0;
  for (const auto& item : root["timeline"]) {
    const std::string context = "timeline[" + std::to_string(index) + "]";
    ++index;
    if (!item.IsMap() || !item["at"]) {
      result.errors.push_back(context + " 缺少 at");
      continue;
    }
    ScenarioEvent event;
    try {
      const double at_seconds = item["at"].as<double>();
      event.at = sentry_decision::Duration{static_cast<std::int64_t>(at_seconds * 1000.0 + 0.5)};
    } catch (const std::exception&) {
      result.errors.push_back(context + ".at 必须是数值（秒）");
      continue;
    }

    if (item["set_world"]) {
      event.kind = ScenarioEvent::Kind::kSetWorld;
      parse_args(item["set_world"], context + ".set_world", &event.args, &result.errors);
    } else if (item["expect"]) {
      event.kind = ScenarioEvent::Kind::kExpect;
      parse_args(item["expect"], context + ".expect", &event.args, &result.errors);
    } else if (item["add_intent"]) {
      event.kind = ScenarioEvent::Kind::kAddIntent;
      parse_args(item["add_intent"], context + ".add_intent", &event.args, &result.errors);
    } else if (item["disable"]) {
      event.kind = ScenarioEvent::Kind::kDisable;
      parse_args(item["disable"], context + ".disable", &event.args, &result.errors);
    } else {
      result.errors.push_back(context + " 需要 set_world / expect / add_intent / disable");
      continue;
    }
    if (event.at > result.scenario.end) {
      result.scenario.end = event.at;
    }
    result.scenario.events.push_back(std::move(event));
  }

  if (result.scenario.events.empty()) {
    result.errors.push_back("场景没有任何 timeline 事件");
  }
  std::stable_sort(result.scenario.events.begin(), result.scenario.events.end(),
                   [](const ScenarioEvent& a, const ScenarioEvent& b) { return a.at < b.at; });
  return result;
}

}  // namespace sentry_decision_sim
