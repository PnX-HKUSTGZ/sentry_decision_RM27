#include "sentry_decision_sim/referee_simulator.hpp"

#include <chrono>

namespace sentry_decision_sim {

RefereeSimulator::RefereeSimulator() {
  // 一个可用的初始世界，便于直接接入；不设 valid，update 后才生效。
  state_.self_hp = 400;
  state_.self_ammo = 100;
  state_.base_hp = 5000;
  state_.our_outpost_hp = 1500;
  state_.enemy_outpost_hp = 1500;
  state_.enemy_base_hp = 5000;
  state_.valid = false;
}

void RefereeSimulator::schedule(sentry_decision::Duration at, Action action) {
  schedule_.emplace_back(at, std::move(action));
  fired_.push_back(false);
}

void RefereeSimulator::update(sentry_decision::TimePoint now) {
  if (!started_) {
    epoch_ = now;
    started_ = true;
  }
  const auto elapsed = std::chrono::duration_cast<sentry_decision::Duration>(now - epoch_);
  for (std::size_t i = 0; i < schedule_.size(); ++i) {
    if (!fired_[i] && schedule_[i].first <= elapsed) {
      fired_[i] = true;
      schedule_[i].second(state_);
    }
  }
  state_.stamp = now;
  state_.valid = true;
}

bool RefereeSimulator::referee(sentry_decision::RefereeState* out) const {
  if (!state_.valid) {
    return false;
  }
  *out = state_;
  return true;
}

}  // namespace sentry_decision_sim
