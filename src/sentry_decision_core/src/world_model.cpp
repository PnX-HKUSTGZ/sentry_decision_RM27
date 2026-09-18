#include "sentry_decision_core/world_model.hpp"

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {
namespace {

bool fresh(TimePoint stamp, TimePoint now, Duration timeout) {
  return now <= stamp + timeout;
}

}  // namespace

WorldModel::WorldModel(RefereeSource& referee, OdometrySource& odometry,
                       NavigationSink& navigation, WorldTimeouts timeouts)
    : referee_(referee),
      odometry_(odometry),
      navigation_(navigation),
      timeouts_(timeouts) {}

WorldState WorldModel::snapshot(TimePoint now) {
  WorldState state;
  state.stamp = now;

  RefereeState referee;
  const bool has_referee = referee_.referee(&referee);
  referee.valid = has_referee && referee.valid && fresh(referee.stamp, now, timeouts_.referee);
  state.referee = referee;
  if (referee_was_valid_ != referee.valid) {
    if (referee.valid) {
      SD_LOG_ACT("world_model", "referee data recovered");
    } else {
      SD_LOG_WARN("world_model", "referee data stale");
    }
    referee_was_valid_ = referee.valid;
  }

  SelfState self;
  const bool has_odometry = odometry_.odometry(&self);
  self.valid = has_odometry && self.valid && fresh(self.stamp, now, timeouts_.odometry);
  state.self = self;
  if (odometry_was_valid_ != self.valid) {
    if (self.valid) {
      SD_LOG_ACT("world_model", "odometry recovered");
    } else {
      SD_LOG_WARN("world_model", "odometry stale");
    }
    odometry_was_valid_ = self.valid;
  }

  NavState nav = navigation_.status();
  nav.valid = nav.valid && fresh(nav.stamp, now, timeouts_.navigation);
  state.nav = nav;
  if (navigation_was_valid_ != nav.valid) {
    if (nav.valid) {
      SD_LOG_ACT("world_model", "navigation state recovered");
    } else {
      SD_LOG_WARN("world_model", "navigation state stale");
    }
    navigation_was_valid_ = nav.valid;
  }

  return state;
}

}  // namespace sentry_decision
