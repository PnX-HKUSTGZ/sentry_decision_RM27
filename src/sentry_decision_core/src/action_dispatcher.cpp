#include "sentry_decision_core/action_dispatcher.hpp"

#include <algorithm>

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {

namespace {

bool is_level(DecisionActionKind kind) {
  return kind == DecisionActionKind::kFreeResurrect ||
         kind == DecisionActionKind::kInstantResurrect;
}

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

// 累计字段的取值上限：发弹量字段 11 bit，但 2027 规则表 5-7 限定 17mm 兑换上限 1000 发/队；
// 远程兑换次数为 4 bit。
constexpr int kAmmoExchangeCap = 1000;
constexpr int kRemoteCountCap = 15;

}  // namespace

bool action_effective(DecisionActionKind kind, int target, const UpstreamState& baseline,
                      const UpstreamState& now) {
  switch (kind) {
    case DecisionActionKind::kAmmoExchange:
      return now.info1.ammo_exchanged_local >= target;
    case DecisionActionKind::kRemoteAmmoExchange:
      return now.info1.remote_ammo_exchange_count >= target;
    case DecisionActionKind::kRemoteHpExchange:
      return now.info1.remote_health_exchange_count >= target;
    case DecisionActionKind::kFreeResurrect:
    case DecisionActionKind::kInstantResurrect:
      // 协议没有「已确认复活」回传；唯一可观测的结果是血量从 0 变为正。
      return baseline.self_hp <= 0 && now.self_hp > 0;
    case DecisionActionKind::kNone:
    default:
      return false;
  }
}

ActionDispatcher::ActionDispatcher(ActionDispatcherConfig config) : config_(config) {}

void ActionDispatcher::submit(const DecisionAction& action) {
  if (action.kind == DecisionActionKind::kNone || action.value <= 0) {
    return;
  }
  Slot& slot = slots_[action.kind];
  slot.desired = action;
  slot.desired_this_tick = true;
}

Duration ActionDispatcher::delay_for(DecisionActionKind kind) const {
  return is_level(kind) ? config_.revive_verify_delay : config_.verify_delay;
}

int ActionDispatcher::max_attempts_for(DecisionActionKind kind) const {
  // 免费复活要在整个读条期间持续置位，默认不限次数；立即复活可立即观测，按普通上限。
  if (kind == DecisionActionKind::kFreeResurrect) {
    return config_.revive_max_attempts;
  }
  return config_.max_attempts;
}

int ActionDispatcher::compute_target(DecisionActionKind kind, int delta,
                                     const UpstreamState& upstream) {
  if (is_level(kind)) {
    return delta > 0 ? 1 : 0;
  }
  int base = 0;
  int cap = 0;
  switch (kind) {
    case DecisionActionKind::kAmmoExchange:
      base = upstream.info1.ammo_exchanged_local;
      cap = kAmmoExchangeCap;
      break;
    case DecisionActionKind::kRemoteAmmoExchange:
      base = upstream.info1.remote_ammo_exchange_count;
      cap = kRemoteCountCap;
      break;
    case DecisionActionKind::kRemoteHpExchange:
      base = upstream.info1.remote_health_exchange_count;
      cap = kRemoteCountCap;
      break;
    default:
      return delta;
  }
  // 0x020D 的累计值只会在同一局内递增；离开「比赛中」视为新一局，重置下界。
  // 比赛中即使上行短暂滞后（base 小于已下发目标）也绝不回退，避免协议判定非法。
  if (upstream.game_status != GameStatus::kRunning) {
    high_water_.erase(kind);
  }
  auto it = high_water_.find(kind);
  const int high_water = it == high_water_.end() ? 0 : it->second;
  const int start = std::max(base, high_water);
  const int target = std::min(cap, start + std::max(delta, 0));
  high_water_[kind] = target;
  return target;
}

void ActionDispatcher::send(Slot& slot, DecisionActionKind kind, TimePoint now,
                            std::vector<DecisionAction>* outgoing) {
  DecisionAction action;
  action.kind = kind;
  action.value = slot.target;
  action.request_id = slot.request_id;
  outgoing->push_back(action);
  slot.last_sent = now;
  ++slot.attempts;
}

void ActionDispatcher::send_clear(DecisionActionKind kind,
                                  std::vector<DecisionAction>* outgoing) {
  // 清零必须用新的 request_id：若 MCU 按 id 去重，复用旧 id 会把清零丢掉。
  DecisionAction clear;
  clear.kind = kind;
  clear.value = 0;
  clear.request_id = next_request_id_++;
  outgoing->push_back(clear);
  SD_LOG_ACT("action", "清除电平动作 %s", kind_name(kind));
}

std::vector<DecisionAction> ActionDispatcher::poll(TimePoint now, const UpstreamState* upstream) {
  std::vector<DecisionAction> outgoing;
  const bool verifying = upstream != nullptr;
  const bool upstream_ok = verifying && upstream->valid;

  for (auto& entry : slots_) {
    const DecisionActionKind kind = entry.first;
    Slot& slot = entry.second;
    if (!slot.desired_this_tick) {
      continue;
    }

    // 值变化 -> 开新的一段请求（即使上一段已生效 / 已放弃）。
    if (!slot.started || slot.desired.value != slot.delta) {
      if (verifying && !upstream_ok) {
        continue;  // 没有有效裁判数据，无法折算累计目标值，等下一拍
      }
      slot.delta = slot.desired.value;
      slot.started = true;
      slot.satisfied = false;
      slot.gave_up = false;
      slot.cleared = false;
      slot.attempts = 0;
      slot.request_id = next_request_id_++;
      slot.baseline = upstream_ok ? *upstream : UpstreamState{};

      if (!verifying) {
        slot.target = slot.delta;
        send(slot, kind, now, &outgoing);
        slot.satisfied = true;  // 不校验模式：只发一次
        continue;
      }

      slot.target = compute_target(kind, slot.delta, *upstream);
      if (is_level(kind) && upstream->self_hp > 0) {
        // 已经复活（例如上行先于本拍提交到达）：没有可做的事，直接视为完成。
        slot.satisfied = true;
        slot.cleared = true;  // 本段从未置位，无需再补发清零
        last_report_ = ActionReport{kind, slot.request_id, 0, true};
        continue;
      }
      if (!is_level(kind) &&
          action_effective(kind, slot.target, slot.baseline, *upstream)) {
        // 目标已经达成（例如进程重启前已兑换成功），不重复下发。
        slot.satisfied = true;
        last_report_ = ActionReport{kind, slot.request_id, 0, true};
        SD_LOG_ACT("action", "动作 %s 目标 %d 已达成，跳过下发", kind_name(kind), slot.target);
        continue;
      }
      send(slot, kind, now, &outgoing);
      SD_LOG_ACT("action", "下发 %s 目标值 %d request_id=%u", kind_name(kind), slot.target,
                 slot.request_id);
      continue;
    }

    if (slot.satisfied || slot.gave_up) {
      continue;
    }
    if (!verifying) {
      continue;
    }
    if (!upstream_ok) {
      continue;  // 链路无效时不做「未生效」判定，避免盲目重发
    }
    if (now - slot.last_sent < delay_for(kind)) {
      continue;
    }

    if (action_effective(kind, slot.target, slot.baseline, *upstream)) {
      slot.satisfied = true;
      last_report_ = ActionReport{kind, slot.request_id, slot.attempts, true};
      SD_LOG_ACT("action", "动作 %s 已生效（第 %d 次，目标 %d）", kind_name(kind), slot.attempts,
                 slot.target);
      if (is_level(kind) && slot.target != 0) {
        send_clear(kind, &outgoing);
        slot.cleared = true;
      }
      continue;
    }

    const int max_attempts = max_attempts_for(kind);
    if (max_attempts > 0 && slot.attempts >= max_attempts) {
      slot.gave_up = true;
      last_report_ = ActionReport{kind, slot.request_id, slot.attempts, false};
      SD_LOG_WARN("action", "动作 %s 未生效，已放弃（共 %d 次，目标 %d）", kind_name(kind),
                  slot.attempts, slot.target);
      if (is_level(kind) && slot.target != 0) {
        send_clear(kind, &outgoing);
        slot.cleared = true;
      }
      continue;
    }

    send(slot, kind, now, &outgoing);
    SD_LOG_DEBUG("action", "动作 %s 未生效，重发第 %d 次（目标 %d）", kind_name(kind),
                 slot.attempts, slot.target);
  }

  // 清理：本 tick 未被提交的条目一律移除；电平动作在请求消失时补发清零。
  for (auto it = slots_.begin(); it != slots_.end();) {
    Slot& slot = it->second;
    if (slot.desired_this_tick) {
      slot.desired_this_tick = false;
      ++it;
      continue;
    }
    // 请求在等待期内消失时也补一次生效判定，供面板观测（兑换类尤其常见）。
    if (slot.started && !slot.gave_up && upstream_ok &&
        action_effective(it->first, slot.target, slot.baseline, *upstream)) {
      last_report_ = ActionReport{it->first, slot.request_id, slot.attempts, true};
      SD_LOG_ACT("action", "动作 %s 已生效（第 %d 次，目标 %d）", kind_name(it->first),
                 slot.attempts, slot.target);
    }
    if (is_level(it->first) && slot.started && slot.target != 0 && !slot.cleared) {
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
  high_water_.clear();
  next_request_id_ = 1;
  last_report_.reset();
}

void submit_resource_requests(ActionDispatcher& dispatcher, const ResourceRequest& resource) {
  const auto submit = [&dispatcher](DecisionActionKind kind, int value) {
    DecisionAction action;
    action.kind = kind;
    action.value = value;
    dispatcher.submit(action);
  };
  // 本地兑换发弹量（需占领增益点）。value 是本次增量，派发器会折算为累计目标值。
  if (resource.ammo > 0) {
    submit(DecisionActionKind::kAmmoExchange, resource.ammo);
  }
  // 远程兑换（需脱战，裁判侧确认后 6s 生效）。
  if (resource.remote_ammo > 0) {
    submit(DecisionActionKind::kRemoteAmmoExchange, resource.remote_ammo);
  }
  if (resource.remote_hp > 0) {
    submit(DecisionActionKind::kRemoteHpExchange, resource.remote_hp);
  }
  // 复活：立即复活优先于确认免费复活。协议 0x0120 bit 0 / bit 1 是电平位：
  // 待复活期间持续置位，复活完成后由派发器补发 0 清零。
  if (resource.instant_revive) {
    submit(DecisionActionKind::kInstantResurrect, 1);
  } else if (resource.revive) {
    submit(DecisionActionKind::kFreeResurrect, 1);
  }
}

}  // namespace sentry_decision
