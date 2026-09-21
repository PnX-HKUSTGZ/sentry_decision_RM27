#pragma once

#include <string>

#include "sentry_decision_core/intervention.hpp"
#include "sentry_decision_core/types.hpp"

namespace sentry_decision_io {

// 一条已解析、待应用到 core::InterventionController 的干预命令。
// action / service 回调只负责产出它并入队；决策 tick 线程负责应用。
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
  sentry_decision::Intent intent;  // kIntent：field / value / lease 已填
  sentry_decision::IntentField intent_field = sentry_decision::IntentField::kNavGoal;
  sentry_decision::WorldField world_field = sentry_decision::WorldField::kSelfHp;
  double world_value = 0.0;
  // 原始取值文本（kIntent / kWorldOverride），用于发布 InterventionEvent 与回放。
  std::string value_text;
  std::string module;
  bool enabled = true;
  std::string reason;
};

}  // namespace sentry_decision_io
