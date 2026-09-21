#include "sentry_decision_sim/decision_actuator_sim.hpp"

namespace sentry_decision_sim {

DecisionActuatorSim::DecisionActuatorSim(int ack_latency_ticks)
    : ack_latency_ticks_(ack_latency_ticks < 0 ? 0 : ack_latency_ticks) {}

void DecisionActuatorSim::send_action(const sentry_decision::DecisionAction& action) {
  in_flight_.push_back(InFlight{action.request_id, ack_latency_ticks_});
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
    sentry_decision::ActionAck ack;
    ack.request_id = item.request_id;
    ack.accepted = true;
    ack.code = 0;
    acks_.push_back(std::move(ack));
  }
  in_flight_.swap(still_in_flight);
}

std::vector<sentry_decision::ActionAck> DecisionActuatorSim::take_acks() {
  std::vector<sentry_decision::ActionAck> acks;
  acks.swap(acks_);
  return acks;
}

void DecisionActuatorSim::reset() {
  in_flight_.clear();
  acks_.clear();
}

}  // namespace sentry_decision_sim
