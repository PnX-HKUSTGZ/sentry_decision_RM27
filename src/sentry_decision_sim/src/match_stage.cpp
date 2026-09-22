#include "sentry_decision_sim/match_stage.hpp"

namespace sentry_decision_sim {

int stage_duration_seconds(MatchStage stage) {
  switch (stage) {
    case MatchStage::kSelfCheck:
      return 15;
    case MatchStage::kCountdown:
      return 5;
    case MatchStage::kRunning:
      return 420;
    case MatchStage::kPreparation:
    case MatchStage::kNotStarted:
    default:
      return 0;
  }
}

void MatchStageController::enter(MatchStage stage) {
  stage_ = stage;
  remaining_ = stage_duration_seconds(stage);
  counting_ = stage == MatchStage::kSelfCheck || stage == MatchStage::kCountdown ||
              stage == MatchStage::kRunning;
}

bool MatchStageController::set(std::uint8_t stage, std::string* error) {
  if (stage > static_cast<std::uint8_t>(MatchStage::kRunning)) {
    if (error != nullptr) {
      *error = "未知比赛阶段: " + std::to_string(stage);
    }
    return false;
  }
  if (stage == 0) {
    reset();
    return true;
  }
  if (active_ && stage <= static_cast<std::uint8_t>(stage_)) {
    if (error != nullptr) {
      *error = "不能回退到当前或之前的阶段";
    }
    return false;
  }
  active_ = true;
  enter(static_cast<MatchStage>(stage));
  return true;
}

void MatchStageController::reset() {
  active_ = true;
  enter(MatchStage::kNotStarted);
}

void MatchStageController::tick_second() {
  if (!active_ || !counting_) {
    return;
  }
  if (remaining_ > 0) {
    --remaining_;
  }
  if (remaining_ > 0) {
    return;
  }
  if (stage_ == MatchStage::kSelfCheck) {
    enter(MatchStage::kCountdown);
  } else if (stage_ == MatchStage::kCountdown) {
    enter(MatchStage::kRunning);
  } else {
    counting_ = false;
  }
}

}  // namespace sentry_decision_sim
