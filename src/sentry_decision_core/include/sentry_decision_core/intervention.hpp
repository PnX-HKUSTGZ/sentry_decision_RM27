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

// 一条干预命令：意图注入 / 世界覆盖 / 模块开关 / 清空。
// 同时用作 ROS 服务端的入队单元与回放数据的一路输入（core 不依赖 ROS）。
struct InterventionCommand {
  enum class Kind {
    kIntent,
    kClearIntent,
    kWorldOverride,
    kClearWorld,
    kModuleSwitch,
    kClearAll,
  };

  Kind kind = Kind::kIntent;
  Intent intent;  // kIntent：field / value / lease 已填
  IntentField intent_field = IntentField::kNavGoal;
  WorldField world_field = WorldField::kSelfHp;
  double world_value = 0.0;
  // 原始取值文本（kIntent），用于发布 InterventionEvent 与回放重建。
  std::string value_text;
  std::string module;
  bool enabled = true;
  std::string reason;
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

  // 撤销某字段的注入意图（不影响其他字段）。
  void clear_intent(IntentField field);

  void set_world_override(WorldField field, double value);
  void clear_world_override(WorldField field);

  void set_module_enabled(const std::string& module, bool enabled);
  // 未显式设置时默认启用。
  bool module_enabled(const std::string& module) const;

  // 意图字段 -> 产出模块；用于运行期模块开关过滤。
  static const char* module_for_field(IntentField field);
  // 该字段是否允许提交（对应模块未关闭）。未知字段默认允许。
  bool allows(IntentField field) const;

  void clear();

  // 当前未过期的注入意图（lease 判定）。
  std::vector<Intent> active_intents(TimePoint now) const;
  // 把世界覆盖应用到副本（不修改入参）。
  WorldState apply_world(const WorldState& world) const;

  // 供 list_state / 可视化展示。
  const std::map<std::string, bool>& module_switches() const {
    return module_switches_;
  }
  const std::map<WorldField, double>& world_overrides() const {
    return world_overrides_;
  }

 private:
  std::map<IntentField, Intent> intents_;
  std::map<WorldField, double> world_overrides_;
  std::map<std::string, bool> module_switches_;
};

// 把一条命令应用到控制器（供实时 tick 与回放共用，保证两条路径语义一致）。
void apply_intervention(InterventionController* controller, const InterventionCommand& command,
                        TimePoint now);

}  // namespace sentry_decision
