#pragma once

#include "sentry_decision_nodes/common_nodes.hpp"
#include "sentry_decision_nodes/nav_nodes.hpp"

namespace sentry_decision {

// 便捷入口：注册全部已链接模块（仅在无 tree_manifest.yaml 的来源目录回退时使用）。
inline void register_sentry_nodes(BT::BehaviorTreeFactory& factory) {
  register_common_nodes(factory);
  register_nav_nodes(factory);
}

}  // namespace sentry_decision
