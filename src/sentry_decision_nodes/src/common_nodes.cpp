#include "sentry_decision_nodes/common_nodes.hpp"

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_nodes/detail/node_helpers.hpp"

namespace sentry_decision {

// [IfLowHp]
IfLowHp::IfLowHp(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfLowHp::providedPorts() {
  return {BT::InputPort<std::string>("hp_key")};
}

BT::NodeStatus IfLowHp::tick() {
  const auto key = getInput<std::string>("hp_key");
  if (!key) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  const auto threshold = context->config->number(key.value());
  if (!threshold.has_value()) {
    SD_LOG_WARN("nodes", "IfLowHp 缺少配置 key: %s", key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.self_hp <= threshold.value() ? BT::NodeStatus::SUCCESS
                                                             : BT::NodeStatus::FAILURE;
}

// [IfTacticalMode]
IfTacticalMode::IfTacticalMode(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfTacticalMode::providedPorts() {
  return {BT::InputPort<int>("mode")};
}

BT::NodeStatus IfTacticalMode::tick() {
  const auto mode = getInput<int>("mode");
  if (!mode) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  return context->strategy.mode == static_cast<TacticalMode>(mode.value())
             ? BT::NodeStatus::SUCCESS
             : BT::NodeStatus::FAILURE;
}

// [IfEnemyOutpostDead]
IfEnemyOutpostDead::IfEnemyOutpostDead(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList IfEnemyOutpostDead::providedPorts() {
  return {};
}

BT::NodeStatus IfEnemyOutpostDead::tick() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.enemy_outpost_hp <= 0 ? BT::NodeStatus::SUCCESS
                                                      : BT::NodeStatus::FAILURE;
}

// [EmitTacticalMode]
EmitTacticalMode::EmitTacticalMode(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList EmitTacticalMode::providedPorts() {
  return {BT::InputPort<int>("mode")};
}

BT::NodeStatus EmitTacticalMode::tick() {
  const auto mode = getInput<int>("mode");
  if (!mode) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  Intent intent;
  intent.field = IntentField::kTacticalMode;
  intent.source = SourceId::kStrategic;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = static_cast<TacticalMode>(mode.value());
  context->emit(intent);
  return BT::NodeStatus::SUCCESS;
}

// [EmitNavGoal]
EmitNavGoal::EmitNavGoal(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList EmitNavGoal::providedPorts() {
  return {BT::InputPort<double>("x"), BT::InputPort<double>("y")};
}

BT::NodeStatus EmitNavGoal::tick() {
  const auto x = getInput<double>("x");
  const auto y = getInput<double>("y");
  if (!x || !y) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  Intent intent;
  intent.field = IntentField::kNavGoal;
  intent.source = SourceId::kSkill;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = Point2D{x.value(), y.value(), 0.0};
  context->emit(intent);
  return BT::NodeStatus::SUCCESS;
}

void register_common_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<IfLowHp>("IfLowHp");
  factory.registerNodeType<IfTacticalMode>("IfTacticalMode");
  factory.registerNodeType<IfEnemyOutpostDead>("IfEnemyOutpostDead");
  factory.registerNodeType<EmitTacticalMode>("EmitTacticalMode");
  factory.registerNodeType<EmitNavGoal>("EmitNavGoal");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_common_nodes(factory);
}
