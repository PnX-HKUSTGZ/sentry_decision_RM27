#include "sentry_decision_sim/match_stage.hpp"

#include <utility>

namespace sentry_decision_sim {

MatchStageController::MatchStageController(MatchDurations durations)
    : durations_(std::move(durations)) {}

int MatchStageController::duration_seconds(MatchStage stage) const {
  switch (stage) {
    case MatchStage::kPreparation:
      return durations_.preparation_s;
    case MatchStage::kSelfCheck:
      return durations_.self_check_s;
    case MatchStage::kCountdown:
      return durations_.countdown_s;
    case MatchStage::kRunning:
      return durations_.running_s;
    case MatchStage::kSettling:
      return durations_.settling_s;
    case MatchStage::kNotStarted:
    default:
      return 0;
  }
}

void MatchStageController::enter(MatchStage stage) {
  stage_ = stage;
  remaining_ = duration_seconds(stage);
  counting_ = stage == MatchStage::kPreparation || stage == MatchStage::kSelfCheck ||
              stage == MatchStage::kCountdown || stage == MatchStage::kRunning ||
              stage == MatchStage::kSettling;
}

bool MatchStageController::set(std::uint8_t stage, std::string* error) {
  if (stage > static_cast<std::uint8_t>(MatchStage::kSettling)) {
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
  if (!active_ || !counting_ || paused_) {
    return;
  }
  if (remaining_ > 0) {
    --remaining_;
  }
  if (remaining_ > 0) {
    return;
  }
  if (stage_ == MatchStage::kPreparation) {
    enter(MatchStage::kSelfCheck);
  } else if (stage_ == MatchStage::kSelfCheck) {
    enter(MatchStage::kCountdown);
  } else if (stage_ == MatchStage::kCountdown) {
    enter(MatchStage::kRunning);
  } else if (stage_ == MatchStage::kRunning) {
    enter(MatchStage::kSettling);  // 比赛时间耗尽 -> 结算
  } else {
    counting_ = false;
  }
}

}  // namespace sentry_decision_sim
