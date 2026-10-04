#pragma once

#include <cstdint>
#include <string>

namespace sentry_decision_sim {

// 比赛阶段，数值与 sentry_decision::GameStatus / GameInfo.game_status 对齐。
enum class MatchStage : std::uint8_t {
  kNotStarted = 0,
  kPreparation = 1,
  kSelfCheck = 2,
  kCountdown = 3,
  kRunning = 4,
  kSettling = 5,  // 比赛结算中：比赛时间耗尽后自动进入
};

// 各阶段展示时长（秒）；0 表示不计时。数值由 config/sim.yaml 的 match 段提供，
// 源码不再硬编码。
struct MatchDurations {
  int preparation_s = 0;
  int self_check_s = 0;
  int countdown_s = 0;
  int running_s = 0;
  int settling_s = 0;
};

// 比赛阶段状态机：只允许前进，stage 0 作为「重置到未开始」。
//
// 未被 set()/reset() 激活前处于 inactive，此时不干预世界（场景 set_world 可自由
// 设置 game_status / game_time_remaining）；一旦激活，就由本类统一驱动阶段与剩余时间。
class MatchStageController {
 public:
  explicit MatchStageController(MatchDurations durations);

  // 设置阶段；stage=0 表示重置。非法取值或回退返回 false 并写 error，状态不变。
  bool set(std::uint8_t stage, std::string* error);
  // 重置到未开始并激活。
  void reset();
  // 推进 1 秒（计时 + 自动进入下一阶段）；由调用方按真实秒调用。
  void tick_second();

  bool active() const {
    return active_;
  }
  MatchStage stage() const {
    return stage_;
  }
  int remaining_seconds() const {
    return remaining_;
  }
  bool counting() const {
    return counting_;
  }
  int duration_seconds(MatchStage stage) const;
  const MatchDurations& durations() const {
    return durations_;
  }

 private:
  void enter(MatchStage stage);

  MatchDurations durations_;
  bool active_ = false;
  MatchStage stage_ = MatchStage::kNotStarted;
  int remaining_ = 0;
  bool counting_ = false;
};

}  // namespace sentry_decision_sim
