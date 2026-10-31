#include "sentry_decision_nodes/resource_nodes.hpp"

#include "sentry_decision_core/action_factory.hpp"
#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_nodes/detail/node_helpers.hpp"

namespace sentry_decision {
namespace {

void emit_resource(DecisionContext* context, const ResourceRequest& request,
                   const ActionRequest& action) {
  ResourceIntent resource;
  resource.request = request;
  resource.action = action;

  Intent intent;
  intent.field = IntentField::kResourceRequest;
  intent.source = SourceId::kSkill;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = resource;
  context->emit(intent);
}

int int_port_or_negative(BT::TreeNode& node, const char* key) {
  const auto value = node.getInput<int>(key);
  return value ? value.value() : -1;
}

// 解析重试策略：树端口优先，其次 config key；两者都没有则失败（配置错误）。
bool resolve_policy(const DecisionContext* context, const char* node, const char* retry_key,
                    const char* attempts_key, int retry_override_ms, int attempts_override,
                    Duration* retry, int* max_attempts) {
  if (retry_override_ms >= 0) {
    *retry = Duration{retry_override_ms};
  } else if (context != nullptr && context->config != nullptr) {
    const auto value = context->config->number(retry_key);
    if (!value.has_value()) {
      SD_LOG_WARN("resource", "%s 缺少配置 key: %s", node, retry_key);
      return false;
    }
    *retry = Duration{static_cast<std::int64_t>(value.value())};
  } else {
    return false;
  }

  if (attempts_override >= 0) {
    *max_attempts = attempts_override;
  } else if (context != nullptr && context->config != nullptr) {
    const auto value = context->config->number(attempts_key);
    if (!value.has_value()) {
      SD_LOG_WARN("resource", "%s 缺少配置 key: %s", node, attempts_key);
      return false;
    }
    *max_attempts = static_cast<int>(value.value());
  } else {
    return false;
  }
  return true;
}

}  // namespace

// [IfCanFreeResurrect]
IfCanFreeResurrect::IfCanFreeResurrect(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfCanFreeResurrect::providedPorts() {
  return {};
}

BT::NodeStatus IfCanFreeResurrect::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.info1.can_free_resurrect ? BT::NodeStatus::SUCCESS
                                                          : BT::NodeStatus::FAILURE;
}

// [IfLowAmmo]
IfLowAmmo::IfLowAmmo(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfLowAmmo::providedPorts() {
  return {BT::InputPort<std::string>("ammo_key")};
}

BT::NodeStatus IfLowAmmo::tick() {
  const auto key = getInput<std::string>("ammo_key");
  if (!key) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const auto threshold = context->config->number(key.value());
  if (!threshold.has_value()) {
    SD_LOG_WARN("resource", "IfLowAmmo 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.self_ammo <= threshold.value() ? BT::NodeStatus::SUCCESS
                                                                : BT::NodeStatus::FAILURE;
}

// [IfCoinsAtLeast]
IfCoinsAtLeast::IfCoinsAtLeast(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfCoinsAtLeast::providedPorts() {
  return {BT::InputPort<std::string>("coins_key")};
}

BT::NodeStatus IfCoinsAtLeast::tick() {
  const auto key = getInput<std::string>("coins_key");
  if (!key) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const auto threshold = context->config->number(key.value());
  if (!threshold.has_value()) {
    SD_LOG_WARN("resource", "IfCoinsAtLeast 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.coins >= threshold.value() ? BT::NodeStatus::SUCCESS
                                                            : BT::NodeStatus::FAILURE;
}

// [IfOccupyingGainPoint]
IfOccupyingGainPoint::IfOccupyingGainPoint(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfOccupyingGainPoint::providedPorts() {
  return {};
}

BT::NodeStatus IfOccupyingGainPoint::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.event.local_ammo_exchange_point() ? BT::NodeStatus::SUCCESS
                                                                   : BT::NodeStatus::FAILURE;
}

// [IfDisengaged]
IfDisengaged::IfDisengaged(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfDisengaged::providedPorts() {
  return {};
}

BT::NodeStatus IfDisengaged::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.info2.disengaged ? BT::NodeStatus::SUCCESS
                                                  : BT::NodeStatus::FAILURE;
}

// [IfNotOccupyingGainPoint]
IfNotOccupyingGainPoint::IfNotOccupyingGainPoint(const std::string& name,
                                                 const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfNotOccupyingGainPoint::providedPorts() {
  return {};
}

BT::NodeStatus IfNotOccupyingGainPoint::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.upstream.event.local_ammo_exchange_point() ? BT::NodeStatus::FAILURE
                                                                   : BT::NodeStatus::SUCCESS;
}

// [IfCanInstantResurrect]
IfCanInstantResurrect::IfCanInstantResurrect(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfCanInstantResurrect::providedPorts() {
  return {};
}

BT::NodeStatus IfCanInstantResurrect::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const UpstreamState& upstream = context->world.upstream;
  const bool affordable = upstream.coins >= static_cast<int>(upstream.info1.instant_resurrect_cost);
  return (upstream.info1.can_instant_resurrect && affordable) ? BT::NodeStatus::SUCCESS
                                                              : BT::NodeStatus::FAILURE;
}

// [RequestFreeRevive]
RequestFreeRevive::RequestFreeRevive(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestFreeRevive::providedPorts() {
  return {BT::InputPort<int>("retry_ms"), BT::InputPort<int>("max_attempts")};
}

BT::NodeStatus RequestFreeRevive::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  Duration retry;
  int max_attempts = 0;
  if (!resolve_policy(context, "RequestFreeRevive", "action.revive_verify_delay_ms",
                      "action.revive_max_attempts", int_port_or_negative(*this, "retry_ms"),
                      int_port_or_negative(*this, "max_attempts"), &retry, &max_attempts)) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.revive = true;
  emit_resource(context, request, make_free_resurrect(retry, max_attempts));
  return BT::NodeStatus::SUCCESS;
}

namespace {

// 读取一个「次数」配置 key，非正或缺失返回 false。
bool times_from_config(const BT::NodeConfig& config, const std::string& key, const char* node,
                       int* out) {
  DecisionContext* context = context_from(config);
  if (context == nullptr || context->config == nullptr) {
    return false;
  }
  const auto value = context->config->number(key);
  if (!value.has_value() || value.value() <= 0.0) {
    SD_LOG_WARN("resource", "%s 缺少或非法配置 key: %s", node, key.c_str());
    return false;
  }
  *out = static_cast<int>(value.value());
  return true;
}

}  // namespace

// [RequestRemoteHpExchange]
RequestRemoteHpExchange::RequestRemoteHpExchange(const std::string& name,
                                                 const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestRemoteHpExchange::providedPorts() {
  return {BT::InputPort<std::string>("times_key"), BT::InputPort<int>("times"),
          BT::InputPort<int>("retry_ms"), BT::InputPort<int>("max_attempts")};
}

BT::NodeStatus RequestRemoteHpExchange::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  int times = int_port_or_negative(*this, "times");
  if (times < 0) {
    const auto key = getInput<std::string>("times_key");
    if (!key || !times_from_config(config(), key.value(), "RequestRemoteHpExchange", &times)) {
      return BT::NodeStatus::FAILURE;
    }
  }
  Duration retry;
  int max_attempts = 0;
  if (!resolve_policy(context, "RequestRemoteHpExchange", "action.verify_delay_ms",
                      "action.max_attempts", int_port_or_negative(*this, "retry_ms"),
                      int_port_or_negative(*this, "max_attempts"), &retry, &max_attempts)) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.remote_hp = times;
  emit_resource(context, request,
                make_remote_hp_exchange(context->world.upstream, times, retry, max_attempts));
  return BT::NodeStatus::SUCCESS;
}

// [RequestRemoteAmmoExchange]
RequestRemoteAmmoExchange::RequestRemoteAmmoExchange(const std::string& name,
                                                     const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestRemoteAmmoExchange::providedPorts() {
  return {BT::InputPort<std::string>("times_key"), BT::InputPort<int>("times"),
          BT::InputPort<int>("retry_ms"), BT::InputPort<int>("max_attempts")};
}

BT::NodeStatus RequestRemoteAmmoExchange::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  int times = int_port_or_negative(*this, "times");
  if (times < 0) {
    const auto key = getInput<std::string>("times_key");
    if (!key || !times_from_config(config(), key.value(), "RequestRemoteAmmoExchange", &times)) {
      return BT::NodeStatus::FAILURE;
    }
  }
  Duration retry;
  int max_attempts = 0;
  if (!resolve_policy(context, "RequestRemoteAmmoExchange", "action.verify_delay_ms",
                      "action.max_attempts", int_port_or_negative(*this, "retry_ms"),
                      int_port_or_negative(*this, "max_attempts"), &retry, &max_attempts)) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.remote_ammo = times;
  emit_resource(context, request,
                make_remote_ammo_exchange(context->world.upstream, times, retry, max_attempts));
  return BT::NodeStatus::SUCCESS;
}

// [RequestInstantRevive]
RequestInstantRevive::RequestInstantRevive(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestInstantRevive::providedPorts() {
  return {BT::InputPort<int>("retry_ms"), BT::InputPort<int>("max_attempts")};
}

BT::NodeStatus RequestInstantRevive::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  Duration retry;
  int max_attempts = 0;
  if (!resolve_policy(context, "RequestInstantRevive", "action.revive_verify_delay_ms",
                      "action.max_attempts", int_port_or_negative(*this, "retry_ms"),
                      int_port_or_negative(*this, "max_attempts"), &retry, &max_attempts)) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.instant_revive = true;
  emit_resource(context, request, make_instant_resurrect(retry, max_attempts));
  return BT::NodeStatus::SUCCESS;
}

// [RequestAmmoExchange]
RequestAmmoExchange::RequestAmmoExchange(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestAmmoExchange::providedPorts() {
  return {BT::InputPort<std::string>("amount_key"), BT::InputPort<int>("amount"),
          BT::InputPort<int>("retry_ms"), BT::InputPort<int>("max_attempts")};
}

BT::NodeStatus RequestAmmoExchange::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr || !context->world.upstream.valid) {
    return BT::NodeStatus::FAILURE;
  }
  int amount = int_port_or_negative(*this, "amount");
  if (amount < 0) {
    const auto key = getInput<std::string>("amount_key");
    if (!key) {
      return BT::NodeStatus::FAILURE;
    }
    const auto configured = context->config->number(key.value());
    if (!configured.has_value()) {
      SD_LOG_WARN("resource", "RequestAmmoExchange 缺少配置 key: %s", key.value().c_str());
      return BT::NodeStatus::FAILURE;
    }
    amount = static_cast<int>(configured.value());
  }
  Duration retry;
  int max_attempts = 0;
  if (!resolve_policy(context, "RequestAmmoExchange", "action.verify_delay_ms",
                      "action.max_attempts", int_port_or_negative(*this, "retry_ms"),
                      int_port_or_negative(*this, "max_attempts"), &retry, &max_attempts)) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.ammo = amount;
  emit_resource(context, request,
                make_ammo_exchange(context->world.upstream, amount, retry, max_attempts));
  return BT::NodeStatus::SUCCESS;
}

void register_resource_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<IfCanFreeResurrect>("IfCanFreeResurrect");
  factory.registerNodeType<IfLowAmmo>("IfLowAmmo");
  factory.registerNodeType<IfCoinsAtLeast>("IfCoinsAtLeast");
  factory.registerNodeType<IfOccupyingGainPoint>("IfOccupyingGainPoint");
  factory.registerNodeType<IfDisengaged>("IfDisengaged");
  factory.registerNodeType<IfNotOccupyingGainPoint>("IfNotOccupyingGainPoint");
  factory.registerNodeType<IfCanInstantResurrect>("IfCanInstantResurrect");
  factory.registerNodeType<RequestFreeRevive>("RequestFreeRevive");
  factory.registerNodeType<RequestInstantRevive>("RequestInstantRevive");
  factory.registerNodeType<RequestRemoteHpExchange>("RequestRemoteHpExchange");
  factory.registerNodeType<RequestRemoteAmmoExchange>("RequestRemoteAmmoExchange");
  factory.registerNodeType<RequestAmmoExchange>("RequestAmmoExchange");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_resource_nodes(factory);
}
