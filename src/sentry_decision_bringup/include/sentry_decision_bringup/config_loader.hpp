#pragma once

#include <string>
#include <vector>

#include "sentry_decision_core/config.hpp"

namespace sentry_decision_bringup {

// 配置加载结果：errors 非空即失败，调用方应打印并退出。
struct ConfigLoadResult {
  sentry_decision::PolicyConfig config;
  std::vector<std::string> errors;

  bool ok() const {
    return errors.empty();
  }
};

// 读取配置入口 profiles.yaml，并据其加载同目录下的 maps/<map>.yaml 与 policies/<strategy>.yaml。
// 纯解析 + 校验，不依赖 ROS。
ConfigLoadResult load_policy_config(const std::string& profiles_path);

// 生效配置的简短摘要，用于启动时的 ACT 日志。
std::string format_config(const sentry_decision::PolicyConfig& config);

}  // namespace sentry_decision_bringup
