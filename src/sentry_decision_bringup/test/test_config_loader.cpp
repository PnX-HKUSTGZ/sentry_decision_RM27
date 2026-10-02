#include <cstdio>
#include <string>

#include "sentry_decision_bringup/config_loader.hpp"

// 地图点位半径（第 4 位）解析：带半径的点进 point_radius，普通点为 0。
int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: %s <profiles.yaml>\n", argv[0]);
    return 2;
  }
  const sentry_decision_bringup::ConfigLoadResult result =
      sentry_decision_bringup::load_policy_config(argv[1]);
  if (!result.ok()) {
    for (const auto& error : result.errors) {
      std::printf("error: %s\n", error.c_str());
    }
    return 1;
  }

  const auto sim = result.config.point_radius.find("sim_home");
  if (sim == result.config.point_radius.end() || sim->second != 1.5) {
    std::printf("FAIL: sim_home 半径应为 1.5\n");
    return 1;
  }
  const auto home = result.config.point_radius.find("home");
  if (home == result.config.point_radius.end() || home->second != 0.0) {
    std::printf("FAIL: 普通点 home 半径应为 0\n");
    return 1;
  }
  std::printf("test_config_loader passed\n");
  return 0;
}
