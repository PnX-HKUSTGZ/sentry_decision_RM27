#include "sentry_decision_core/intervention.hpp"

namespace sentry_decision {

void InterventionController::inject(Intent intent, TimePoint now) {
  intent.source = SourceId::kIntervention;
  intent.priority = Priority::kIntervention;
  intent.stamp = now;
  intents_[intent.field] = intent;
}

void InterventionController::clear_intent(IntentField field) {
  intents_.erase(field);
}

void InterventionController::set_world_override(WorldField field, double value) {
  world_overrides_[field] = value;
}

void InterventionController::clear_world_override(WorldField field) {
  world_overrides_.erase(field);
}

void InterventionController::set_module_enabled(const std::string& module, bool enabled) {
  module_switches_[module] = enabled;
}

bool InterventionController::module_enabled(const std::string& module) const {
  const auto it = module_switches_.find(module);
  return it == module_switches_.end() ? true : it->second;
}

const char* InterventionController::module_for_field(IntentField field) {
  switch (field) {
    case IntentField::kNavGoal:
      return "nav";
    case IntentField::kChassisVel:
      return "recovery";
    case IntentField::kResourceRequest:
      return "resource";
    case IntentField::kTacticalMode:
      return "strategic";
  }
  return "";
}

bool InterventionController::allows(IntentField field) const {
  const char* module = module_for_field(field);
  return module[0] == '\0' ? true : module_enabled(module);
}

void InterventionController::clear() {
  intents_.clear();
  world_overrides_.clear();
  module_switches_.clear();
}

std::vector<Intent> InterventionController::active_intents(TimePoint now) const {
  std::vector<Intent> active;
  for (const auto& entry : intents_) {
    const Intent& intent = entry.second;
    if (intent.lease.count() == 0 || now <= intent.stamp + intent.lease) {
      active.push_back(intent);
    }
  }
  return active;
}

void apply_intervention(InterventionController* controller, const InterventionCommand& command,
                        TimePoint now) {
  if (controller == nullptr) {
    return;
  }
  switch (command.kind) {
    case InterventionCommand::Kind::kIntent:
      controller->inject(command.intent, now);
      break;
    case InterventionCommand::Kind::kClearIntent:
      controller->clear_intent(command.intent_field);
      break;
    case InterventionCommand::Kind::kWorldOverride:
      controller->set_world_override(command.world_field, command.world_value);
      break;
    case InterventionCommand::Kind::kClearWorld:
      controller->clear_world_override(command.world_field);
      break;
    case InterventionCommand::Kind::kModuleSwitch:
      controller->set_module_enabled(command.module, command.enabled);
      break;
    case InterventionCommand::Kind::kClearAll:
      controller->clear();
      break;
  }
}

WorldState InterventionController::apply_world(const WorldState& world) const {
  WorldState out = world;
  for (const auto& entry : world_overrides_) {
    const double value = entry.second;
    switch (entry.first) {
      // 注意：世界覆盖只改数值，不提升 referee.valid——否则会绕过 SafetySupervisor
      // 的裁判失效急停。失效态仍由原始输入决定。
      case WorldField::kSelfHp:
        out.referee.self_hp = static_cast<int>(value);
        break;
      case WorldField::kSelfAmmo:
        out.referee.self_ammo = static_cast<int>(value);
        break;
      case WorldField::kOurOutpostHp:
        out.referee.our_outpost_hp = static_cast<int>(value);
        break;
      case WorldField::kEnemyOutpostHp:
        out.referee.enemy_outpost_hp = static_cast<int>(value);
        break;
      case WorldField::kGameTimeRemaining:
        out.referee.game_time_remaining = static_cast<int>(value);
        break;
      case WorldField::kCoins:
        out.referee.coins = static_cast<int>(value);
        break;
    }
  }
  return out;
}

}  // namespace sentry_decision
