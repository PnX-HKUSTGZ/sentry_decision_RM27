#include "sentry_decision_core/world_model.hpp"

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {
namespace {

bool fresh(TimePoint stamp, TimePoint now, Duration timeout) {
  return now <= stamp + timeout;
}

}  // namespace

WorldModel::WorldModel(UpstreamSource& upstream, OdometrySource& odometry, NavigationSink& navigation,
                       WorldTimeouts timeouts)
    : upstream_(upstream), odometry_(odometry), navigation_(navigation), timeouts_(timeouts) {}

WorldState WorldModel::snapshot(TimePoint now) {
  WorldState state;
  state.stamp = now;

  UpstreamState upstream;
  const bool has_upstream = upstream_.upstream(&upstream);
  upstream.valid = has_upstream && upstream.valid && fresh(upstream.stamp, now, timeouts_.upstream);
  state.upstream = upstream;
  if (upstream_was_valid_ != upstream.valid) {
    if (upstream.valid) {
      SD_LOG_ACT("world_model", "upstream data recovered");
    } else {
      SD_LOG_WARN("world_model", "upstream data stale");
    }
    upstream_was_valid_ = upstream.valid;
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
