#include "sentry_decision_nodes/nodes.hpp"

namespace sentry_decision {
namespace {

DecisionContext* context_from(const BT::NodeConfig& config) {
  if (!config.blackboard) {
    return nullptr;
  }
  return config.blackboard->get<DecisionContext*>("context");
}

}  // namespace

CheckLowHp::CheckLowHp(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList CheckLowHp::providedPorts() {
  return {BT::InputPort<int>("hp_threshold")};
}

BT::NodeStatus CheckLowHp::tick() {
  const auto threshold = getInput<int>("hp_threshold");
  if (!threshold) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || !context->world.referee.valid) {
    return BT::NodeStatus::FAILURE;
  }
  return context->world.referee.self_hp <= threshold.value() ? BT::NodeStatus::SUCCESS
                                                             : BT::NodeStatus::FAILURE;
}

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

void register_sentry_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<CheckLowHp>("CheckLowHp");
  factory.registerNodeType<EmitTacticalMode>("EmitTacticalMode");
  factory.registerNodeType<EmitNavGoal>("EmitNavGoal");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_sentry_nodes(factory);
}
