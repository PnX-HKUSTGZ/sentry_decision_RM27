#include "sentry_decision_core/intervention.hpp"

namespace sentry_decision {

void InterventionController::inject(Intent intent, TimePoint now) {
  intent.source = SourceId::kIntervention;
  intent.priority = Priority::kIntervention;
  intent.stamp = now;
  intents_[intent.field] = intent;
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

WorldState InterventionController::apply_world(const WorldState& world) const {
  WorldState out = world;
  for (const auto& entry : world_overrides_) {
    const double value = entry.second;
    switch (entry.first) {
      case WorldField::kSelfHp:
        out.referee.self_hp = static_cast<int>(value);
        out.referee.valid = true;
        break;
      case WorldField::kSelfAmmo:
        out.referee.self_ammo = static_cast<int>(value);
        out.referee.valid = true;
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
