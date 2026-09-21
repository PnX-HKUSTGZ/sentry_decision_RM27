#include "sentry_decision_nodes/resource_nodes.hpp"

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_nodes/detail/node_helpers.hpp"

namespace sentry_decision {
namespace {

void emit_resource(DecisionContext* context, const ResourceRequest& request) {
  Intent intent;
  intent.field = IntentField::kResourceRequest;
  intent.source = SourceId::kSkill;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = request;
  context->emit(intent);
}

}  // namespace

IfCanFreeResurrect::IfCanFreeResurrect(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfCanFreeResurrect::providedPorts() {
  return {};
}

BT::NodeStatus IfCanFreeResurrect::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.info1.can_free_resurrect ? BT::NodeStatus::SUCCESS
                                                         : BT::NodeStatus::FAILURE;
}

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
  if (context == nullptr || context->config == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const auto threshold = context->config->number(key.value());
  if (!threshold.has_value()) {
    SD_LOG_WARN("resource", "IfLowAmmo 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.self_ammo <= threshold.value() ? BT::NodeStatus::SUCCESS
                                                               : BT::NodeStatus::FAILURE;
}

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
  if (context == nullptr || context->config == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const auto threshold = context->config->number(key.value());
  if (!threshold.has_value()) {
    SD_LOG_WARN("resource", "IfCoinsAtLeast 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.coins >= threshold.value() ? BT::NodeStatus::SUCCESS
                                                           : BT::NodeStatus::FAILURE;
}

RequestFreeRevive::RequestFreeRevive(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestFreeRevive::providedPorts() {
  return {};
}

BT::NodeStatus RequestFreeRevive::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.revive = true;
  emit_resource(context, request);
  return BT::NodeStatus::SUCCESS;
}

RequestHpExchange::RequestHpExchange(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestHpExchange::providedPorts() {
  return {BT::InputPort<std::string>("amount_key")};
}

BT::NodeStatus RequestHpExchange::tick() {
  const auto key = getInput<std::string>("amount_key");
  if (!key) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  const auto amount = context->config->number(key.value());
  if (!amount.has_value()) {
    SD_LOG_WARN("resource", "RequestHpExchange 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.hp = static_cast<int>(amount.value());
  emit_resource(context, request);
  return BT::NodeStatus::SUCCESS;
}

RequestAmmoExchange::RequestAmmoExchange(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList RequestAmmoExchange::providedPorts() {
  return {BT::InputPort<std::string>("amount_key")};
}

BT::NodeStatus RequestAmmoExchange::tick() {
  const auto key = getInput<std::string>("amount_key");
  if (!key) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  const auto amount = context->config->number(key.value());
  if (!amount.has_value()) {
    SD_LOG_WARN("resource", "RequestAmmoExchange 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  ResourceRequest request;
  request.ammo = static_cast<int>(amount.value());
  emit_resource(context, request);
  return BT::NodeStatus::SUCCESS;
}

void register_resource_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<IfCanFreeResurrect>("IfCanFreeResurrect");
  factory.registerNodeType<IfLowAmmo>("IfLowAmmo");
  factory.registerNodeType<IfCoinsAtLeast>("IfCoinsAtLeast");
  factory.registerNodeType<RequestFreeRevive>("RequestFreeRevive");
  factory.registerNodeType<RequestHpExchange>("RequestHpExchange");
  factory.registerNodeType<RequestAmmoExchange>("RequestAmmoExchange");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_resource_nodes(factory);
}
