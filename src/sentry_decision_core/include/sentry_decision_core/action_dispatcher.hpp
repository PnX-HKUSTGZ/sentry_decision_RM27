#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

struct ActionDispatcherConfig {
  // one-shot 发出后超过该时长仍未收到 ack，记一次 WARN（重发策略待协议确定）。
  Duration one_shot_timeout{500};
};

// 决策动作派发器：把决策层每 tick 的期望动作转成「本 tick 实际要发送的命令」，
// 并维护 one-shot / polled 语义与 ack 确认。纯逻辑、无 ROS，可在宿主机单测。
//
// 用法（同一 tick 线程内）：
//   dispatcher.submit(action);                      // 每 tick 提交期望（可多次）
//   for (auto& action : dispatcher.poll(now)) {      // 本 tick 待发送
//     sink.send_action(action);
//   }
//   dispatcher.on_ack(ack);                         // 收到回执
//
// 语义：
//   - one-shot：同一 (kind, value) 只在「由无到有」时发送一次；收到 ack 后不重发；
//     决策层停止提交后条目清除，将来同值再次提交视为新请求。
//   - polled：只要本 tick 仍在提交，就按 interval 重发（interval=0 表示每 tick）；
//     停止提交即停止重发。
class ActionDispatcher {
 public:
  explicit ActionDispatcher(ActionDispatcherConfig config = {});

  void submit(const DecisionAction& action);
  void on_ack(const ActionAck& ack);
  std::vector<DecisionAction> poll(TimePoint now);
  void reset();

 private:
  struct Slot {
    DecisionAction desired{};
    bool desired_this_tick = false;
    bool ever_sent = false;
    int sent_value = 0;
    TimePoint last_sent{};
    bool pending_ack = false;
    TimePoint pending_since{};
    bool timeout_warned = false;
    std::uint32_t last_sent_id = 0;
  };

  ActionDispatcherConfig config_;
  std::map<DecisionActionKind, Slot> slots_;
  std::uint32_t next_request_id_ = 1;
};

// 把仲裁后的资源请求转成一次性决策动作并提交（仅提交非零字段）。
void submit_resource_requests(ActionDispatcher& dispatcher, const ResourceRequest& resource);

}  // namespace sentry_decision
