#include "sentry_decision_bringup/config_loader.hpp"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace sentry_decision_bringup {
namespace {

namespace fs = std::filesystem;

std::string join_key(const std::string& prefix, const std::string& key) {
  return prefix.empty() ? key : prefix + "." + key;
}

// 把嵌套 map 的标量叶子展开成点分 key；布尔进 flags，数值进 numbers。
void flatten_scalars(const YAML::Node& node, const std::string& prefix,
                     sentry_decision::PolicyConfig* config, std::vector<std::string>* errors) {
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      flatten_scalars(it->second, join_key(prefix, it->first.as<std::string>()), config, errors);
    }
    return;
  }
  if (!node.IsScalar()) {
    errors->push_back("配置项 " + prefix + " 结构不支持（需要标量或嵌套 map）");
    return;
  }
  std::string text;
  try {
    text = node.as<std::string>();
  } catch (const std::exception&) {
    errors->push_back("配置项 " + prefix + " 无法读取");
    return;
  }
  if (text == "true" || text == "false") {
    config->flags[prefix] = (text == "true");
    return;
  }
  try {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    // 只允许尾部空白：必须完整消费字符串，避免 "50oops" 被当作 50。
    std::size_t tail = consumed;
    while (tail < text.size() && std::isspace(static_cast<unsigned char>(text[tail])) != 0) {
      ++tail;
    }
    if (tail == text.size() && std::isfinite(value)) {
      config->numbers[prefix] = value;
      return;
    }
  } catch (const std::exception&) {
    // 落到下面的报错
  }
  errors->push_back("配置项 " + prefix + " 不是可识别的数值 / 布尔: " + text);
}

bool load_point(const YAML::Node& node, const std::string& name, sentry_decision::Point2D* out,
                std::vector<std::string>* errors) {
  try {
    if (node.IsSequence()) {
      if (node.size() < 2 || node.size() > 3) {
        errors->push_back("点位 " + name + " 需要 [x, y] 或 [x, y, yaw]");
        return false;
      }
      out->x = node[0].as<double>();
      out->y = node[1].as<double>();
      out->yaw = node.size() == 3 ? node[2].as<double>() : 0.0;
    } else if (node.IsMap()) {
      // 点位是外部输入：map 形式必须显式给出 x 与 y，缺失即报错，不静默落到原点。
      if (!node["x"] || !node["y"]) {
        errors->push_back("点位 " + name + " 的 map 形式必须包含 x 和 y");
        return false;
      }
      out->x = node["x"].as<double>();
      out->y = node["y"].as<double>();
      out->yaw = node["yaw"] ? node["yaw"].as<double>() : 0.0;
    } else {
      errors->push_back("点位 " + name + " 格式不支持");
      return false;
    }
  } catch (const std::exception&) {
    errors->push_back("点位 " + name + " 含非数值");
    return false;
  }
  if (!std::isfinite(out->x) || !std::isfinite(out->y) || !std::isfinite(out->yaw)) {
    errors->push_back("点位 " + name + " 含非有限值");
    return false;
  }
  return true;
}

}  // namespace

ConfigLoadResult load_policy_config(const std::string& profiles_path) {
  ConfigLoadResult result;
  const fs::path profiles(profiles_path);
  YAML::Node root;
  try {
    root = YAML::LoadFile(profiles_path);
  } catch (const std::exception& ex) {
    result.errors.push_back(std::string("读取配置入口失败: ") + ex.what());
    return result;
  }

  auto require_string = [&](const char* key, std::string* out) -> bool {
    if (!root[key] || !root[key].IsScalar()) {
      result.errors.push_back(std::string("profiles.yaml 缺少 ") + key);
      return false;
    }
    *out = root[key].as<std::string>();
    return true;
  };

  if (!require_string("map_profile", &result.config.map_profile) ||
      !require_string("strategy_profile", &result.config.strategy_profile)) {
    return result;
  }

  if (root["pre_match"] && root["pre_match"].IsMap()) {
    flatten_scalars(root["pre_match"], "pre_match", &result.config, &result.errors);
  }

  const fs::path dir = profiles.parent_path();
  const fs::path map_path = dir / "maps" / (result.config.map_profile + ".yaml");
  const fs::path policy_path = dir / "policies" / (result.config.strategy_profile + ".yaml");

  try {
    YAML::Node map = YAML::LoadFile(map_path.string());
    if (map["frame_id"] && map["frame_id"].IsScalar()) {
      result.config.map_frame = map["frame_id"].as<std::string>();
    } else {
      result.errors.push_back("地图配置缺少 frame_id: " + map_path.string());
    }
    if (map["points"] && map["points"].IsMap()) {
      for (auto it = map["points"].begin(); it != map["points"].end(); ++it) {
        const std::string name = it->first.as<std::string>();
        sentry_decision::Point2D point;
        if (load_point(it->second, name, &point, &result.errors)) {
          result.config.points[name] = point;
        }
      }
    } else {
      result.errors.push_back("地图配置缺少 points: " + map_path.string());
    }
  } catch (const std::exception& ex) {
    result.errors.push_back("读取地图配置失败(" + map_path.string() + "): " + ex.what());
  }

  try {
    YAML::Node policy = YAML::LoadFile(policy_path.string());
    flatten_scalars(policy, "", &result.config, &result.errors);
  } catch (const std::exception& ex) {
    result.errors.push_back("读取策略配置失败(" + policy_path.string() + "): " + ex.what());
  }

  return result;
}

std::string format_config(const sentry_decision::PolicyConfig& config) {
  std::ostringstream out;
  out << "map=" << config.map_profile << " strategy=" << config.strategy_profile
      << " frame=" << config.map_frame << " points=" << config.points.size()
      << " numbers=" << config.numbers.size() << " flags=" << config.flags.size();
  return out.str();
}

}  // namespace sentry_decision_bringup
