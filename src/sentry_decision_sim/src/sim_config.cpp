#include "sentry_decision_sim/sim_config.hpp"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <map>
#include <string>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision_sim {
namespace {

double yaml_number(const YAML::Node& node, const char* key, double fallback) {
  return node[key] ? node[key].as<double>() : fallback;
}

int yaml_int(const YAML::Node& node, const char* key, int fallback) {
  return node[key] ? node[key].as<int>() : fallback;
}

// 读取一个「point + radius」形式的增益点；point 必须在命名点表中。
bool load_zone(const YAML::Node& node,
               const std::map<std::string, sentry_decision::Point2D>& points, const char* label,
               double* x, double* y, double* radius, std::string* error) {
  if (!node) {
    return true;
  }
  const std::string point = node["point"] ? node["point"].as<std::string>() : "";
  const auto it = points.find(point);
  if (it == points.end()) {
    *error = std::string(label) + ": 未知命名点 '" + point + "'";
    return false;
  }
  *x = it->second.x;
  *y = it->second.y;
  if (node["radius"]) {
    *radius = node["radius"].as<double>();
  }
  return true;
}

}  // namespace

bool load_sim_config(const std::string& path, SimConfig* out, std::string* error) {
  try {
    const YAML::Node root = YAML::LoadFile(path);
    const std::filesystem::path dir = std::filesystem::path(path).parent_path();
    std::map<std::string, sentry_decision::Point2D> points;
    const std::string map_rel = root["map"] ? root["map"].as<std::string>() : "";
    if (!map_rel.empty()) {
      const YAML::Node map = YAML::LoadFile((dir / map_rel).string());
      for (const auto& item : map["points"]) {
        const YAML::Node value = item.second;
        if (!value.IsSequence() || value.size() < 2) {
          *error = "地图点位 " + item.first.as<std::string>() + " 需要 [x, y]";
          return false;
        }
        sentry_decision::Point2D p;
        p.x = value[0].as<double>();
        p.y = value[1].as<double>();
        p.yaw = value.size() > 2 ? value[2].as<double>() : 0.0;
        points[item.first.as<std::string>()] = p;
      }
    }
    out->max_hp = yaml_int(root, "max_hp", out->max_hp);
    if (root["nav"]) {
      out->nav_speed = yaml_number(root["nav"], "speed_mps", out->nav_speed);
      out->nav_tolerance = yaml_number(root["nav"], "tolerance_m", out->nav_tolerance);
    }
    if (root["match"]) {
      const YAML::Node match = root["match"];
      out->match.preparation_s = yaml_int(match, "preparation_s", out->match.preparation_s);
      out->match.self_check_s = yaml_int(match, "self_check_s", out->match.self_check_s);
      out->match.countdown_s = yaml_int(match, "countdown_s", out->match.countdown_s);
      out->match.running_s = yaml_int(match, "running_s", out->match.running_s);
      out->match.settling_s = yaml_int(match, "settling_s", out->match.settling_s);
    }
    if (root["supply"]) {
      const YAML::Node supply = root["supply"];
      if (!load_zone(supply, points, "supply", &out->supply_x, &out->supply_y, &out->supply_radius,
                     error)) {
        return false;
      }
      out->supply_heal_ratio = yaml_number(supply, "heal_ratio", out->supply_heal_ratio);
      out->supply_heal_ratio_late =
          yaml_number(supply, "heal_ratio_late", out->supply_heal_ratio_late);
      out->supply_heal_late_after_s =
          yaml_number(supply, "heal_late_after_s", out->supply_heal_late_after_s);
    }
    if (root["gain_zones"]) {
      const YAML::Node zones = root["gain_zones"];
      if (!load_zone(zones["base_buff"], points, "base_buff", &out->base_x, &out->base_y,
                     &out->base_radius, error) ||
          !load_zone(zones["our_outpost"], points, "our_outpost", &out->our_x, &out->our_y,
                     &out->our_radius, error) ||
          !load_zone(zones["fort_buff"], points, "fort_buff", &out->fort_x, &out->fort_y,
                     &out->fort_radius, error)) {
        return false;
      }
    }
    return true;
  } catch (const std::exception& ex) {
    *error = ex.what();
    return false;
  }
}

}  // namespace sentry_decision_sim
