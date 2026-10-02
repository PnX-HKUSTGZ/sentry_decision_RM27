#include "sentry_decision_sim/sim_config.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
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

// 读取一个增益点：point 必须在命名点表中，半径取自地图点第 4 位（必须为正）。
bool load_zone(const YAML::Node& node,
               const std::map<std::string, sentry_decision::Point2D>& points,
               const std::map<std::string, double>& radii, const char* label, double* x, double* y,
               double* radius, std::string* error) {
  if (!node) {
    return true;
  }
  const std::string point = node["point"] ? node["point"].as<std::string>() : "";
  const auto it = points.find(point);
  if (it == points.end()) {
    *error = std::string(label) + ": 未知命名点 '" + point + "'";
    return false;
  }
  const auto radius_it = radii.find(point);
  if (radius_it == radii.end() || !(radius_it->second > 0.0)) {
    *error = std::string(label) + ": 地图点 '" + point + "' 缺少正半径（点第 4 位）";
    return false;
  }
  *x = it->second.x;
  *y = it->second.y;
  *radius = radius_it->second;
  return true;
}

}  // namespace

bool load_sim_config(const std::string& path, SimConfig* out, std::string* error) {
  try {
    const YAML::Node root = YAML::LoadFile(path);
    const std::filesystem::path dir = std::filesystem::path(path).parent_path();
    std::map<std::string, sentry_decision::Point2D> points;
    std::map<std::string, double> radii;
    const std::string map_rel = root["map"] ? root["map"].as<std::string>() : "";
    if (!map_rel.empty()) {
      const YAML::Node map = YAML::LoadFile((dir / map_rel).string());
      for (const auto& item : map["points"]) {
        const YAML::Node value = item.second;
        const std::string name = item.first.as<std::string>();
        if (!value.IsSequence() || value.size() < 2 || value.size() > 4) {
          *error = "地图点位 " + name + " 需要 [x, y] / [x, y, yaw] / [x, y, yaw, radius]";
          return false;
        }
        sentry_decision::Point2D p;
        p.x = value[0].as<double>();
        p.y = value[1].as<double>();
        p.yaw = value.size() > 2 ? value[2].as<double>() : 0.0;
        const double radius = value.size() == 4 ? value[3].as<double>() : 0.0;
        if (!std::isfinite(radius) || radius < 0.0) {
          *error = "地图点位 " + name + " 的半径必须是非负有限值";
          return false;
        }
        points[name] = p;
        radii[name] = radius;
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
      if (!load_zone(supply, points, radii, "supply", &out->supply_x, &out->supply_y,
                     &out->supply_radius, error)) {
        return false;
      }
      out->supply_enter_delay_s = yaml_number(supply, "enter_delay_s", out->supply_enter_delay_s);
      out->supply_heal_ratio = yaml_number(supply, "heal_ratio", out->supply_heal_ratio);
      out->supply_heal_ratio_late =
          yaml_number(supply, "heal_ratio_late", out->supply_heal_ratio_late);
      out->supply_heal_late_after_s =
          yaml_number(supply, "heal_late_after_s", out->supply_heal_late_after_s);
    }
    if (root["gain_zones"]) {
      const YAML::Node zones = root["gain_zones"];
      if (!load_zone(zones["base_buff"], points, radii, "base_buff", &out->base_x, &out->base_y,
                     &out->base_radius, error) ||
          !load_zone(zones["our_outpost"], points, radii, "our_outpost", &out->our_x, &out->our_y,
                     &out->our_radius, error) ||
          !load_zone(zones["fort_buff"], points, radii, "fort_buff", &out->fort_x, &out->fort_y,
                     &out->fort_radius, error)) {
        return false;
      }
    }
    if (root["effects"]) {
      const YAML::Node effects = root["effects"];
      out->effects.self_damage = yaml_number(effects, "self_damage", out->effects.self_damage);
      out->effects.self_ammo_consume =
          yaml_number(effects, "self_ammo_consume", out->effects.self_ammo_consume);
      out->effects.our_outpost_damage =
          yaml_number(effects, "our_outpost_damage", out->effects.our_outpost_damage);
      out->effects.our_base_damage =
          yaml_number(effects, "our_base_damage", out->effects.our_base_damage);
      out->effects.enemy_outpost_damage =
          yaml_number(effects, "enemy_outpost_damage", out->effects.enemy_outpost_damage);
      out->effects.enemy_base_damage =
          yaml_number(effects, "enemy_base_damage", out->effects.enemy_base_damage);
    }
    return true;
  } catch (const std::exception& ex) {
    *error = ex.what();
    return false;
  }
}

}  // namespace sentry_decision_sim
