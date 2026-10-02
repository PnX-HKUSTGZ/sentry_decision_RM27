#include "sentry_decision_core/replay.hpp"

#include <algorithm>

namespace sentry_decision {

ReplaySource::ReplaySource(ReplayData data, TimePoint epoch)
    : data_(std::move(data)), epoch_(epoch) {
  const auto max_of = [](const auto& records) {
    Duration last{};
    for (const auto& record : records) {
      last = std::max(last, record.at);
    }
    return last;
  };
  last_at_ = std::max({max_of(data_.referee), max_of(data_.odometry), max_of(data_.navigation),
                       max_of(data_.interventions)});
}

Duration ReplaySource::step(Duration period) {
  now_ += period;
  ++tick_;
  return now_;
}

bool ReplaySource::referee(RefereeState* out) const {
  const ReplayRecord<RefereeState>* record = latest(data_.referee, now_);
  if (record == nullptr) {
    return false;
  }
  *out = record->value;
  out->stamp = epoch_ + record->at;
  return true;
}

bool ReplaySource::odometry(SelfState* out) const {
  const ReplayRecord<SelfState>* record = latest(data_.odometry, now_);
  if (record == nullptr) {
    return false;
  }
  *out = record->value;
  out->stamp = epoch_ + record->at;
  return true;
}

NavState ReplaySource::status() const {
  const ReplayRecord<NavState>* record = latest(data_.navigation, now_);
  if (record == nullptr) {
    return NavState{};
  }
  NavState state = record->value;
  state.stamp = epoch_ + record->at;
  return state;
}

std::vector<InterventionCommand> ReplaySource::interventions() {
  std::vector<InterventionCommand> due;
  while (intervention_cursor_ < data_.interventions.size() &&
         data_.interventions[intervention_cursor_].at <= now_) {
    due.push_back(data_.interventions[intervention_cursor_].value);
    ++intervention_cursor_;
  }
  return due;
}

void ReplaySource::send_goal(const Point2D& goal) {
  ++sent_goals_;
  last_goal_ = goal;
}

void ReplaySource::cancel_goal() {
  ++canceled_goals_;
}

}  // namespace sentry_decision
