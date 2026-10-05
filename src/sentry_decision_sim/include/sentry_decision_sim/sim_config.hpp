#pragma once

#include <string>

#include "sentry_decision_sim/match_stage.hpp"

namespace sentry_decision_sim {

// 仿真效果步长（面板「仿真效果」按钮）：对真实世界做减法；摧毁类直接置 0。
struct SimEffects {
  double self_damage = 0.0;
  double self_ammo_consume = 0.0;
  double our_outpost_damage = 0.0;
  double our_base_damage = 0.0;
  double enemy_outpost_damage = 0.0;
  double enemy_base_damage = 0.0;
};

// 裁判仿真参数：默认值全部来自 config/sim.yaml（点位来自其引用的 map），源码不硬编码。
struct SimConfig {
  int max_hp = 0;
  double nav_speed = 0.0;
  double nav_tolerance = 0.0;
  MatchDurations match;
  double supply_x = 0.0;
  double supply_y = 0.0;
  double supply_radius = 0.0;
  // 进入补给区后延迟多少秒才开始回血（0 = 立即）。
  double supply_enter_delay_s = 0.0;
  double supply_heal_ratio = 0.0;
  double supply_heal_ratio_late = 0.0;
  double supply_heal_late_after_s = 0.0;
  // 双仓库模式重置时的回起点坐标（命名点，默认 map 的 start）。
  double start_x = 0.0, start_y = 0.0;
  double base_x = 0.0, base_y = 0.0, base_radius = 0.0;
  double our_x = 0.0, our_y = 0.0, our_radius = 0.0;
  double fort_x = 0.0, fort_y = 0.0, fort_radius = 0.0;
  SimEffects effects;
};

// 读取 sim yaml（其中 map: 引用的命名点用于解析增益点圆心）。失败返回 false 并写 error。
bool load_sim_config(const std::string& path, SimConfig* out, std::string* error);

}  // namespace sentry_decision_sim
