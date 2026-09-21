#pragma once

#include <utility>
#include <vector>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/strategic_policy.hpp"
#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 一次 tick 的决策上下文：当前世界状态 + 本 tick 的意图缓冲。
// 行为树节点只读写它，不直接接触 ROS。
struct DecisionContext {
  WorldState world;
  // 只读配置；由组合根在启动时加载并注入，节点用于解析命名点 / 配置 key。
  const PolicyConfig* config = nullptr;
  // 本 tick 的战略层结论；任务树读取它做任务选择。
  StrategicDecision strategy;
  std::vector<Intent> intents;

  void clear_intents() {
    intents.clear();
  }

  // 应用战略层结论：写入 strategy，并提交 kTacticalMode / kStance 意图（owner = kStrategic），
  // 使仲裁输出与可视化能看到战术模式与期望姿态。
  void apply_strategy(const StrategicDecision& decision) {
    strategy = decision;
    Intent mode_intent;
    mode_intent.field = IntentField::kTacticalMode;
    mode_intent.source = SourceId::kStrategic;
    mode_intent.priority = Priority::kTactical;
    mode_intent.stamp = world.stamp;
    mode_intent.value = decision.mode;
    emit(std::move(mode_intent));

    Intent stance_intent;
    stance_intent.field = IntentField::kStance;
    stance_intent.source = SourceId::kStrategic;
    stance_intent.priority = Priority::kTactical;
    stance_intent.stamp = world.stamp;
    stance_intent.value = decision.stance;
    emit(std::move(stance_intent));
  }

  void emit(Intent intent) {
    intents.push_back(std::move(intent));
  }
};

}  // namespace sentry_decision
