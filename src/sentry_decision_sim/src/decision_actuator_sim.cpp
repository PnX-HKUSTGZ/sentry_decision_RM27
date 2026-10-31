#include "sentry_decision_sim/decision_actuator_sim.hpp"

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision_sim {

DecisionActuatorSim::DecisionActuatorSim(int latency_ticks)
    : latency_ticks_(latency_ticks < 0 ? 0 : latency_ticks) {}

void DecisionActuatorSim::bind_world(SimWorld* world, int max_hp) {
  world_ = world;
  max_hp_ = max_hp;
}

void DecisionActuatorSim::send_action(const sentry_decision::DecisionAction& action) {
  in_flight_.push_back(InFlight{action, latency_ticks_});
}

void DecisionActuatorSim::update() {
  std::vector<InFlight> still_in_flight;
  still_in_flight.reserve(in_flight_.size());
  for (auto& item : in_flight_) {
    if (item.remaining > 0) {
      --item.remaining;
    }
    if (item.remaining > 0) {
      still_in_flight.push_back(item);
      continue;
    }
    if (world_ == nullptr) {
      continue;
    }
    const ActionOutcome outcome = execute_action(world_, item.action, max_hp_);
    if (!outcome.accepted) {
      // 裁判是动作合法性的权威：拒绝时留下日志，供仿真排查。
      SD_LOG_WARN("sim", "裁判拒绝动作 request_id=%u kind=%d code=%u %s",
                  item.action.request_id, static_cast<int>(item.action.kind),
                  static_cast<unsigned>(outcome.code), outcome.detail.c_str());
    }
  }
  in_flight_.swap(still_in_flight);
}

void DecisionActuatorSim::reset() {
  in_flight_.clear();
}

}  // namespace sentry_decision_sim
