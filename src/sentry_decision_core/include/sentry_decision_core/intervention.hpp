#pragma once

#include <map>
#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 可注入覆盖的世界状态字段（类型化，避免字符串反射）。
enum class WorldField {
  kSelfHp,
  kSelfAmmo,
  kOurOutpostHp,
  kEnemyOutpostHp,
  kGameTimeRemaining,
  kCoins,
};

// 人工干预（P2 core 侧）：意图注入 + 世界状态覆盖 + 模块开关。
// 结构化 action / service 与网页面板留 P3；本类只提供纯逻辑入口，可宿主单测。
//
// 约束：注入的意图统一使用 intervention 来源与优先级（硬安全仍最高），
// 因此干预走同一条仲裁流水线，不能绕过 SafetySupervisor。
class InterventionController {
 public:
  // 注入一条带 lease 的意图覆盖（按 field 覆盖前值）；source / priority / stamp 由本类设置。
  void inject(Intent intent, TimePoint now);

  void set_world_override(WorldField field, double value);
  void clear_world_override(WorldField field);

  void set_module_enabled(const std::string& module, bool enabled);
  // 未显式设置时默认启用。
  bool module_enabled(const std::string& module) const;

  void clear();

  // 当前未过期的注入意图（lease 判定）。
  std::vector<Intent> active_intents(TimePoint now) const;
  // 把世界覆盖应用到副本（不修改入参）。
  WorldState apply_world(const WorldState& world) const;

 private:
  std::map<IntentField, Intent> intents_;
  std::map<WorldField, double> world_overrides_;
  std::map<std::string, bool> module_switches_;
};

}  // namespace sentry_decision
