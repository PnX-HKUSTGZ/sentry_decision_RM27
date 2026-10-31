#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 决策动作派发器：只做通用的「下发 -> 等待 -> 校验 -> 重试 / 放弃」。
// 动作语义（目标值、判据、最小单位）由 action_factory 构造，dispatcher 不按 kind 分支。
//
// 语义：
//   - 每段请求只在开始时取一次 target；请求持续期间忽略 target 变化（避免累计值
//     随成功结算而变化时被误判成新请求）。
//   - 发出后等待 request.retry_interval，再用 request.verify 判断；未生效重发同一
//     target，达到 request.max_attempts 后放弃。
//   - request.level 为 true 时，完成 / 放弃 / 请求消失后补发一条 value=0。
//
// 用法（同一 tick 线程内）：
//   dispatcher.submit(action_request);                      // 每 tick 提交期望
//   for (auto& action : dispatcher.poll(now, &upstream)) {  // 本 tick 待发送
//     sink.send_action(action);
//   }
class ActionDispatcher {
 public:
  ActionDispatcher() = default;

  void submit(const ActionRequest& request);

  // upstream == nullptr：不校验，每次「由无到有」只下发一次（回放 / 无裁判数据的演示）。
  // upstream->valid == false：暂停，等待有效数据后再判定或重发。
  std::vector<DecisionAction> poll(TimePoint now, const UpstreamState* upstream);

  // 清除某个动作的「已满足」记忆，使下一次提交视为新请求。
  void rearm(DecisionActionKind kind);
  void reset();

  std::optional<ActionReport> last_report() const {
    return last_report_;
  }

 private:
  struct Slot {
    ActionRequest desired{};  // 本 tick 的期望
    ActionRequest request{};  // 当前这一段请求（target / 策略已锁定）
    bool desired_this_tick = false;
    bool started = false;
    bool satisfied = false;
    bool gave_up = false;
    bool cleared = false;
    int target = 0;
    int attempts = 0;
    std::uint32_t request_id = 0;
    TimePoint last_sent{};
    UpstreamState baseline{};
  };

  bool same_request(const ActionRequest& a, const ActionRequest& b) const;
  void send(Slot& slot, TimePoint now, std::vector<DecisionAction>* outgoing);
  void send_clear(DecisionActionKind kind, std::vector<DecisionAction>* outgoing);

  std::map<DecisionActionKind, Slot> slots_;
  // 累计目标的单调下界：上行短暂滞后时也不让 target 回退。
  std::map<DecisionActionKind, int> last_target_;
  std::uint32_t next_request_id_ = 1;
  std::optional<ActionReport> last_report_;
};

}  // namespace sentry_decision
