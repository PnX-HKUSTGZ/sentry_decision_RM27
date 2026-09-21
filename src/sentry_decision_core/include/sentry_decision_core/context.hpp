#pragma once

#include <utility>
#include <vector>

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 一次 tick 的决策上下文：当前世界状态 + 本 tick 的意图缓冲。
// 行为树节点只读写它，不直接接触 ROS。
struct DecisionContext {
  WorldState world;
  // 只读配置；由组合根在启动时加载并注入，节点用于解析命名点 / 配置 key。
  const PolicyConfig* config = nullptr;
  std::vector<Intent> intents;

  void clear_intents() {
    intents.clear();
  }

  void emit(Intent intent) {
    intents.push_back(std::move(intent));
  }
};

}  // namespace sentry_decision
