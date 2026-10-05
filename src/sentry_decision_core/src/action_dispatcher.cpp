#include "sentry_decision_core/action_dispatcher.hpp"

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {

ActionDispatcher::ActionDispatcher(ActionDispatcherConfig config) : config_(config) {}

void ActionDispatcher::submit(const DecisionAction& action) {
  if (action.kind == DecisionActionKind::kNone) {
    return;
  }
  Slot& slot = slots_[action.kind];
  slot.desired = action;
  slot.desired_this_tick = true;
}

void ActionDispatcher::on_ack(const ActionAck& ack) {
  for (auto& entry : slots_) {
    Slot& slot = entry.second;
    if (slot.last_sent_id == 0 || slot.last_sent_id != ack.request_id) {
      continue;
    }
    if (slot.pending_ack) {
      slot.pending_ack = false;
      slot.timeout_warned = false;
    }
    if (ack.accepted) {
      // 轮询动作每 interval 一条回执，按 DEBUG 记录，避免刷屏（首次 / 关键动作仍走 ACT）。
      if (slot.desired.mode == ActionMode::kPolled) {
        SD_LOG_DEBUG("action", "轮询动作 %d 已确认 request_id=%u", static_cast<int>(entry.first),
                     ack.request_id);
      } else {
        SD_LOG_ACT("action", "动作 %d 已确认 request_id=%u", static_cast<int>(entry.first),
                   ack.request_id);
      }
    } else {
      SD_LOG_WARN("action", "动作 %d 被拒绝 request_id=%u code=%u %s",
                  static_cast<int>(entry.first), ack.request_id, static_cast<unsigned>(ack.code),
                  ack.detail.c_str());
    }
    return;
  }
  SD_LOG_WARN("action", "收到未知 request_id=%u 的回执", ack.request_id);
}

std::vector<DecisionAction> ActionDispatcher::poll(TimePoint now) {
  std::vector<DecisionAction> outgoing;

  for (auto& entry : slots_) {
    Slot& slot = entry.second;
    if (!slot.desired_this_tick) {
      continue;
    }
    const DecisionAction& desired = slot.desired;

    if (desired.mode == ActionMode::kOneShot) {
      // 与上次发送值相同则不重发（由无到有）。
      if (slot.ever_sent && slot.sent_value == desired.value) {
        continue;
      }
      DecisionAction action = desired;
      action.request_id = next_request_id_++;
      outgoing.push_back(action);
      slot.ever_sent = true;
      slot.sent_value = desired.value;
      slot.last_sent = now;
      slot.last_sent_id = action.request_id;
      slot.pending_ack = true;
      slot.pending_since = now;
      slot.timeout_warned = false;
      continue;
    }

    // polled：按 interval 重发。
    const bool due = !slot.ever_sent || desired.interval.count() == 0 ||
                     now >= slot.last_sent + desired.interval;
    if (due) {
      DecisionAction action = desired;
      action.request_id = next_request_id_++;
      outgoing.push_back(action);
      slot.ever_sent = true;
      slot.sent_value = desired.value;
      slot.last_sent = now;
      slot.last_sent_id = action.request_id;
    }
  }

  // one-shot 超时告警（不清除，等待协议确定重发策略）。
  for (auto& entry : slots_) {
    Slot& slot = entry.second;
    if (!slot.pending_ack || slot.timeout_warned || config_.one_shot_timeout.count() <= 0) {
      continue;
    }
    if (now > slot.pending_since + config_.one_shot_timeout) {
      SD_LOG_WARN("action", "动作 %d request_id=%u 超时未确认", static_cast<int>(entry.first),
                  slot.last_sent_id);
      slot.timeout_warned = true;
    }
  }

  // 清理：本 tick 未被提交的条目一律移除。
  // one-shot 的「由无到有」以决策层的提交为准：停止提交后再次提交同值视为新请求；
  // 只要持续提交，超时告警就会持续生效。
  for (auto it = slots_.begin(); it != slots_.end();) {
    if (it->second.desired_this_tick) {
      ++it;
      continue;
    }
    it = slots_.erase(it);
  }

  for (auto& entry : slots_) {
    entry.second.desired_this_tick = false;
  }
  return outgoing;
}

void ActionDispatcher::rearm(DecisionActionKind kind) {
  slots_.erase(kind);
}

void ActionDispatcher::reset() {
  slots_.clear();
  next_request_id_ = 1;
}

void submit_resource_requests(ActionDispatcher& dispatcher, const ResourceRequest& resource) {
  const auto submit = [&dispatcher](DecisionActionKind kind, int value,
                                    ActionMode mode = ActionMode::kOneShot,
                                    Duration interval = Duration{0}) {
    DecisionAction action;
    action.kind = kind;
    action.mode = mode;
    action.interval = interval;
    action.value = value;
    dispatcher.submit(action);
  };
  // 本地兑换发弹量（需占领增益点）。
  if (resource.ammo > 0) {
    submit(DecisionActionKind::kAmmoExchange, resource.ammo);
  }
  // 本地兑换血量：仅人工注入 / 调试会用到（规则只允许远程兑换）。
  if (resource.hp > 0) {
    submit(DecisionActionKind::kHpExchange, resource.hp);
  }
  // 远程兑换（需脱战，裁判侧确认后 6s 生效）。
  if (resource.remote_ammo > 0) {
    submit(DecisionActionKind::kRemoteAmmoExchange, resource.remote_ammo);
  }
  if (resource.remote_hp > 0) {
    submit(DecisionActionKind::kRemoteHpExchange, resource.remote_hp);
  }
  // 复活：立即复活优先于确认免费复活。协议 0x0120 bit 0 / bit 1 是电平位，
  // 用轮询在待复活期间持续置位（停止提交即停止置位），避免单帧丢失导致不复活。
  const Duration revive_interval = dispatcher.revive_poll_interval();
  if (resource.instant_revive) {
    submit(DecisionActionKind::kInstantResurrect, 0, ActionMode::kPolled, revive_interval);
  } else if (resource.revive) {
    submit(DecisionActionKind::kFreeResurrect, 0, ActionMode::kPolled, revive_interval);
  }
}

}  // namespace sentry_decision
