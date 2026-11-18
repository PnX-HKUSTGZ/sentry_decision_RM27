#pragma once

#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/task_runtime.hpp"

namespace sentry_decision {

// 注册 2027 第一版 Task。config 必须比 registry / runtime 活得久。
void register_default_tasks(TaskRegistry& registry, const PolicyConfig& config, int max_hp,
                            Duration match_duration);

}  // namespace sentry_decision
