#include "sentry_decision_core/task_factory.hpp"

#include "sentry_decision_core/tasks/heal_task.hpp"
#include "sentry_decision_core/tasks/navigate_task.hpp"
#include "sentry_decision_core/tasks/outpost_task.hpp"
#include "sentry_decision_core/tasks/resource_task.hpp"
#include "sentry_decision_core/tasks/respawn_task.hpp"
#include "sentry_decision_core/tasks/scan_task.hpp"

namespace sentry_decision {

void register_default_tasks(TaskRegistry& registry, const PolicyConfig& config, int max_hp,
                            Duration match_duration) {
  registry.register_task("Navigate", [&config] { return std::make_unique<NavigateTask>(config); });
  registry.register_task("Scan", [] { return std::make_unique<ScanTask>(); });
  registry.register_task("Outpost", [] { return std::make_unique<OutpostTask>(); });
  registry.register_task("Respawn", [] { return std::make_unique<RespawnTask>(); });
  registry.register_task("Heal", [&config, max_hp, match_duration] {
    return std::make_unique<HealTask>(config, max_hp, match_duration);
  });
  registry.register_task("Resource", [] { return std::make_unique<ResourceTask>(); });
}

}  // namespace sentry_decision
