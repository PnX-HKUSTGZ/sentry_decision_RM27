#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "sentry_decision_core/action_dispatcher.hpp"
#include "sentry_decision_core/config.hpp"
#include "sentry_decision_core/safety_supervisor.hpp"
#include "sentry_decision_core/world_model.hpp"

namespace sentry_decision_bringup {

// 从 PolicyConfig（config/policies/<strategy>.yaml）构造运行期组件配置。
// yaml 是参数的单一事实来源：缺 key 或 config 为空直接抛异常（启动即失败），
// core 结构体不再内置默认值。
inline double config_number(const sentry_decision::PolicyConfig* config, const char* key) {
  if (config == nullptr) {
    throw std::runtime_error("配置为空，无法读取 " + std::string(key));
  }
  const auto value = config->number(key);
  if (!value.has_value()) {
    throw std::runtime_error("缺少配置项: " + std::string(key));
  }
  return *value;
}

inline bool config_flag(const sentry_decision::PolicyConfig* config, const char* key) {
  if (config == nullptr) {
    throw std::runtime_error("配置为空，无法读取 " + std::string(key));
  }
  const auto value = config->flag(key);
  if (!value.has_value()) {
    throw std::runtime_error("缺少配置项: " + std::string(key));
  }
  return *value;
}

inline sentry_decision::Duration ms_from_config(const sentry_decision::PolicyConfig* config,
                                                const char* key) {
  return sentry_decision::Duration{static_cast<std::int64_t>(config_number(config, key))};
}

inline sentry_decision::WorldTimeouts timeouts_from_config(
    const sentry_decision::PolicyConfig* config) {
  sentry_decision::WorldTimeouts timeouts;
  timeouts.referee = ms_from_config(config, "timeouts.referee_ms");
  timeouts.odometry = ms_from_config(config, "timeouts.odometry_ms");
  timeouts.navigation = ms_from_config(config, "timeouts.navigation_ms");
  return timeouts;
}

inline sentry_decision::SafetyLimits safety_limits_from_config(
    const sentry_decision::PolicyConfig* config) {
  sentry_decision::SafetyLimits limits;
  limits.max_vx = config_number(config, "safety.max_vx");
  limits.max_vy = config_number(config, "safety.max_vy");
  limits.max_wz = config_number(config, "safety.max_wz");
  limits.require_referee = config_flag(config, "safety.require_referee");
  limits.require_odometry = config_flag(config, "safety.require_odometry");
  return limits;
}

inline sentry_decision::ActionDispatcherConfig action_config_from_config(
    const sentry_decision::PolicyConfig* config) {
  sentry_decision::ActionDispatcherConfig action_config;
  action_config.one_shot_timeout = ms_from_config(config, "action.one_shot_timeout_ms");
  action_config.revive_poll_interval = ms_from_config(config, "action.revive_poll_interval_ms");
  return action_config;
}

}  // namespace sentry_decision_bringup
