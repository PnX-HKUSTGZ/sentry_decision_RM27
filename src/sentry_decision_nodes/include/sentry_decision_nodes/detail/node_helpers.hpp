#pragma once

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/context.hpp"

namespace sentry_decision {

// 子树有独立黑板（BT.CPP v4），但它是根黑板的子节点，因此统一从根黑板取
// 组合根注入的 DecisionContext 指针。
inline DecisionContext* context_from(const BT::NodeConfig& config) {
  if (!config.blackboard) {
    return nullptr;
  }
  return config.blackboard->rootBlackboard()->get<DecisionContext*>("context");
}

}  // namespace sentry_decision
