#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 动作派发配置（来自 config/policies/<POLICY>.yaml 的 action 段，源码不设默认值）。
//   verify_delay / max_attempts：普通动作（兑换类 / 立即复活）发出后等待多久、最多发几次；
//   revive_verify_delay / revive_max_attempts：复活动作（电平位）的检查间隔与次数上限。
//   revive_max_attempts 只约束「确认免费复活」，0 表示不限次数（读条期间要持续置位）；
//   「兑换立即复活」可立即观测，仍用 max_attempts。
struct ActionDispatcherConfig {
  Duration verify_delay;
  int max_attempts;
  Duration revive_verify_delay;
  int revive_max_attempts;
};

// 判断一次动作请求是否已经生效（纯函数，便于单测）。
// target 是下发时使用的协议累计目标值（复活为 0/1）；baseline 是发起请求时的裁判快照。
bool action_effective(DecisionActionKind kind, int target, const UpstreamState& baseline,
                      const UpstreamState& now);

// 决策动作派发器：把决策层每 tick 的期望动作转成「本 tick 实际要下发的命令」。
// 纯逻辑、无 ROS，可在宿主机单测。
//
// 语义（与裁判协议 0x0120 / 0x020D 对齐）：
//   - 下发值不是增量，而是协议要求的累计目标值：本地兑换发弹量 = 本场累计兑换量，
//     远程兑换 = 累计请求次数，复活 = 电平位 1/0。因此同一目标重复下发是幂等的。
//   - 发出后等待 verify_delay（复活用 revive_verify_delay），再用裁判上行判断是否生效；
//     未生效则重发同一目标，达到次数上限后放弃（等请求消失或 value 变化后重新开始）。
//   - 复活动作在读条期间按 revive_verify_delay 持续置位，直到复活完成 / 不再请求；
//     完成或放弃后补发一条 value=0 清零。
//
// 用法（同一 tick 线程内）：
//   dispatcher.submit(action);                              // 每 tick 提交期望
//   for (auto& action : dispatcher.poll(now, &upstream)) {  // 本 tick 待发送
//     sink.send_action(action);
//   }
class ActionDispatcher {
 public:
  explicit ActionDispatcher(ActionDispatcherConfig config);

  void submit(const DecisionAction& action);

  // upstream == nullptr：不校验，每次「由无到有」只下发一次（回放 / 无裁判数据的演示）。
  // upstream->valid == false：暂停，等待有效数据后再判定或重发。
  std::vector<DecisionAction> poll(TimePoint now, const UpstreamState* upstream);

  // 清除某个动作的「已满足」记忆，使下一次提交视为新请求。
  // 人工干预（如面板重复点「兑换发弹」）需要它：意图持续存在时同值请求本会被去重。
  void rearm(DecisionActionKind kind);
  void reset();

  // 最近一次「生效 / 放弃」的结果，供面板与日志观测。
  std::optional<ActionReport> last_report() const {
    return last_report_;
  }

 private:
  struct Slot {
    DecisionAction desired{};
    bool desired_this_tick = false;
    bool started = false;    // 本段请求已经下发过
    bool satisfied = false;  // 已观测到生效
    bool gave_up = false;    // 达到次数上限仍未生效
    bool cleared = false;    // 电平位已补发过 0
    int delta = 0;           // 决策层给的增量
    int target = 0;          // 协议累计目标值
    int attempts = 0;        // 已下发次数（含首次）
    std::uint32_t request_id = 0;
    TimePoint last_sent{};
    UpstreamState baseline{};
  };

  Duration delay_for(DecisionActionKind kind) const;
  int max_attempts_for(DecisionActionKind kind) const;
  // 增量 -> 协议累计目标值；同一进程内保证单调不减。
  int compute_target(DecisionActionKind kind, int delta, const UpstreamState& upstream);
  void send(Slot& slot, DecisionActionKind kind, TimePoint now,
            std::vector<DecisionAction>* outgoing);
  void send_clear(DecisionActionKind kind, std::vector<DecisionAction>* outgoing);

  ActionDispatcherConfig config_;
  std::map<DecisionActionKind, Slot> slots_;
  std::map<DecisionActionKind, int> high_water_;  // 累计目标的单调下界
  std::uint32_t next_request_id_ = 1;
  std::optional<ActionReport> last_report_;
};

// 把仲裁后的资源请求转成一次性决策动作并提交（仅提交非零字段）。
void submit_resource_requests(ActionDispatcher& dispatcher, const ResourceRequest& resource);

}  // namespace sentry_decision
