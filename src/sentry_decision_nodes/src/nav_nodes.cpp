#include "sentry_decision_nodes/nav_nodes.hpp"

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_nodes/detail/node_helpers.hpp"

namespace sentry_decision {

EmitNavGoalFromPoint::EmitNavGoalFromPoint(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList EmitNavGoalFromPoint::providedPorts() {
  return {BT::InputPort<std::string>("point")};
}

BT::NodeStatus EmitNavGoalFromPoint::tick() {
  const auto point = getInput<std::string>("point");
  if (!point) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  const Point2D* resolved = context->config->find_point(point.value());
  if (resolved == nullptr) {
    SD_LOG_WARN("nodes", "EmitNavGoalFromPoint 缺少命名点: %s", point.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  Intent intent;
  intent.field = IntentField::kNavGoal;
  intent.source = SourceId::kSkill;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = *resolved;
  context->emit(intent);
  return BT::NodeStatus::SUCCESS;
}

void register_nav_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<EmitNavGoalFromPoint>("EmitNavGoalFromPoint");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_nav_nodes(factory);
}
