#pragma once

#include <map>
#include <optional>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 决策动作派发器：每个请求只下发一次，不重试。
// 任务层负责检测窗口与失败判定；dispatcher 只做「去重发送 + 观测报告」。
//
// 语义：
//   - 每段请求只在开始时取一次 target；请求持续期间忽略 target 变化。
//   - 发出后不再重发；后续 poll 只用 verify 观测是否生效。
//   - request.level 为 true 时，完成 / 请求消失后补发一条 value=0。
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
  // upstream->valid == false：暂停，等待有效数据后再发送。
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
    bool sent = false;
    bool done = false;
    bool cleared = false;
    int target = 0;
    std::uint32_t request_id = 0;
    UpstreamState baseline{};
  };

  bool same_request(const ActionRequest& a, const ActionRequest& b) const;
  void send(Slot& slot, std::vector<DecisionAction>* outgoing);
  void send_clear(DecisionActionKind kind, std::vector<DecisionAction>* outgoing);

  std::map<DecisionActionKind, Slot> slots_;
  // 累计目标的单调下界：上行短暂滞后时也不让 target 回退。
  std::map<DecisionActionKind, int> last_target_;
  std::uint32_t next_request_id_ = 1;
  std::optional<ActionReport> last_report_;
};

}  // namespace sentry_decision
