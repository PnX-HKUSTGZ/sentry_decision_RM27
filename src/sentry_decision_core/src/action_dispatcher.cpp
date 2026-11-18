#include "sentry_decision_core/action_dispatcher.hpp"

#include <utility>

#include "sentry_decision_core/action_factory.hpp"
#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {

namespace {

const char* kind_name(DecisionActionKind kind) {
  switch (kind) {
    case DecisionActionKind::kAmmoExchange:
      return "本地兑换发弹量";
    case DecisionActionKind::kFreeResurrect:
      return "确认免费复活";
    case DecisionActionKind::kInstantResurrect:
      return "兑换立即复活";
    case DecisionActionKind::kRemoteAmmoExchange:
      return "远程兑换发弹量";
    case DecisionActionKind::kRemoteHpExchange:
      return "远程兑换血量";
    case DecisionActionKind::kNone:
    default:
      return "无";
  }
}

}  // namespace

void ActionDispatcher::submit(const ActionRequest& request) {
  if (request.kind == DecisionActionKind::kNone || request.target <= 0) {
    return;
  }
  Slot& slot = slots_[request.kind];
  slot.desired = request;
  slot.desired_this_tick = true;
}

bool ActionDispatcher::same_request(const ActionRequest& a, const ActionRequest& b) const {
  // 不比较 target：累计值会随成功结算变化，但同一段请求的 target 已锁定。
  return a.kind == b.kind && a.delta == b.delta && a.verify == b.verify && a.level == b.level &&
         a.monotonic == b.monotonic;
}

void ActionDispatcher::send(Slot& slot, std::vector<DecisionAction>* outgoing) {
  DecisionAction action;
  action.kind = slot.request.kind;
  action.value = slot.target;
  action.request_id = slot.request_id;
  outgoing->push_back(action);
  slot.sent = true;
}

void ActionDispatcher::send_clear(DecisionActionKind kind, std::vector<DecisionAction>* outgoing) {
  // 清零必须用新的 request_id：若 MCU 按 id 去重，复用旧 id 会把清零丢掉。
  DecisionAction clear;
  clear.kind = kind;
  clear.value = 0;
  clear.request_id = next_request_id_++;
  outgoing->push_back(clear);
  SD_LOG_ACT("action", "清除电平动作 %s", kind_name(kind));
}

std::vector<DecisionAction> ActionDispatcher::poll(TimePoint /*now*/,
                                                   const UpstreamState* upstream) {
  std::vector<DecisionAction> outgoing;
  const bool verifying = upstream != nullptr;
  const bool upstream_ok = verifying && upstream->valid;

  // 累计目标只在一局内单调；离开比赛中视为新一局。
  if (upstream_ok && upstream->game_status != GameStatus::kRunning) {
    last_target_.clear();
  }

  for (auto& entry : slots_) {
    const DecisionActionKind kind = entry.first;
    Slot& slot = entry.second;
    if (!slot.desired_this_tick) {
      continue;
    }

    // 请求变化（增量 / 判据 / 电平）才开新的一段；同一段内忽略 target 变化。
    if (!slot.started || !same_request(slot.desired, slot.request)) {
      if (verifying && !upstream_ok) {
        continue;  // 没有有效裁判数据，等下一拍
      }
      slot.request = slot.desired;
      slot.started = true;
      slot.sent = false;
      slot.done = false;
      slot.cleared = false;
      slot.request_id = next_request_id_++;
      slot.baseline = upstream_ok ? *upstream : UpstreamState{};
      slot.target = slot.request.target;

      if (slot.request.monotonic) {
        auto it = last_target_.find(kind);
        if (it != last_target_.end() && slot.target < it->second) {
          slot.target = it->second;
        }
        last_target_[kind] = slot.target;
      }

      if (slot.target <= 0) {
        slot.done = true;
        last_report_ = ActionReport{kind, slot.request_id, false};
        SD_LOG_WARN("action", "动作 %s 请求无效（target=%d），未下发", kind_name(kind),
                    slot.target);
        continue;
      }

      if (upstream_ok && slot.request.level && upstream->self_hp > 0) {
        // 已经复活（例如上行先于本拍提交到达）：没有可做的事。
        slot.done = true;
        slot.cleared = true;
        last_report_ = ActionReport{kind, slot.request_id, true};
        continue;
      }
      if (upstream_ok && !slot.request.level &&
          verify_action(slot.request.verify, slot.target, slot.baseline, *upstream)) {
        // 目标已经达成（例如进程重启前已兑换成功），不重复下发。
        slot.done = true;
        last_report_ = ActionReport{kind, slot.request_id, true};
        SD_LOG_ACT("action", "动作 %s 目标 %d 已达成，跳过下发", kind_name(kind), slot.target);
        continue;
      }

      send(slot, &outgoing);
      SD_LOG_ACT("action", "下发 %s 目标值 %d request_id=%u", kind_name(kind), slot.target,
                 slot.request_id);
      if (!verifying) {
        slot.done = true;
      }
      continue;
    }

    if (slot.done || !verifying || !upstream_ok || !slot.sent) {
      continue;
    }

    if (verify_action(slot.request.verify, slot.target, slot.baseline, *upstream)) {
      slot.done = true;
      last_report_ = ActionReport{kind, slot.request_id, true};
      SD_LOG_ACT("action", "动作 %s 已生效（目标 %d）", kind_name(kind), slot.target);
      if (slot.request.level && slot.target != 0) {
        send_clear(kind, &outgoing);
        slot.cleared = true;
      }
    }
  }

  // 清理：本 tick 未被提交的条目一律移除；电平动作在请求消失时补发清零。
  for (auto it = slots_.begin(); it != slots_.end();) {
    Slot& slot = it->second;
    if (slot.desired_this_tick) {
      slot.desired_this_tick = false;
      ++it;
      continue;
    }
    if (slot.started && slot.sent && !slot.done && upstream_ok &&
        verify_action(slot.request.verify, slot.target, slot.baseline, *upstream)) {
      last_report_ = ActionReport{it->first, slot.request_id, true};
      SD_LOG_ACT("action", "动作 %s 已生效（目标 %d）", kind_name(it->first), slot.target);
    }
    if (slot.request.level && slot.started && !slot.cleared) {
      send_clear(it->first, &outgoing);
    }
    it = slots_.erase(it);
  }
  return outgoing;
}

void ActionDispatcher::rearm(DecisionActionKind kind) {
  slots_.erase(kind);
}

void ActionDispatcher::reset() {
  slots_.clear();
  last_target_.clear();
  next_request_id_ = 1;
  last_report_.reset();
}

}  // namespace sentry_decision
