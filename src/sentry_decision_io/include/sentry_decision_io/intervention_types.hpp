#pragma once

#include "sentry_decision_core/intervention.hpp"

namespace sentry_decision_io {

// 干预命令定义在 core（回放数据也要用）；这里只做别名，保持 io 命名空间可读。
using InterventionCommand = sentry_decision::InterventionCommand;

}  // namespace sentry_decision_io
