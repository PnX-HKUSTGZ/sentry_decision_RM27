#include "sentry_decision_core/action_factory.hpp"

#include <algorithm>

#include "sentry_decision_core/logging.hpp"

namespace sentry_decision {

namespace {

ActionRequest make_base(DecisionActionKind kind, ActionVerify verify, bool level) {
  ActionRequest request;
  request.kind = kind;
  request.verify = verify;
  request.level = level;
  return request;
}

// 远程兑换的协议要求“每次仅能增加 1”；超过 1 次不能合成一条命令。
bool valid_remote_times(int times, const char* action) {
  if (times <= 0) {
    return false;
  }
  if (times > 1) {
    SD_LOG_ERROR("action", "%s 一次只能请求 1 次（当前 %d），请拆成多次或改配置", action, times);
    return false;
  }
  return true;
}

}  // namespace

ActionRequest make_ammo_exchange(const UpstreamState& upstream, int delta) {
  ActionRequest request =
      make_base(DecisionActionKind::kAmmoExchange, ActionVerify::kAmmoExchangedLocal, false);
  if (delta <= 0) {
    return request;
  }
  const int rounded = (delta / kAmmoExchangeUnit) * kAmmoExchangeUnit;
  if (rounded != delta) {
    SD_LOG_WARN("action", "本地兑换发弹量必须是 %d 的倍数（收到 %d），按 %d 处理",
                kAmmoExchangeUnit, delta, rounded);
  }
  if (rounded <= 0) {
    return request;
  }
  request.delta = rounded;
  request.monotonic = true;
  request.target = std::min(kAmmoExchangeCap, upstream.info1.ammo_exchanged_local + rounded);
  return request;
}

ActionRequest make_remote_ammo_exchange(const UpstreamState& upstream, int times) {
  ActionRequest request =
      make_base(DecisionActionKind::kRemoteAmmoExchange, ActionVerify::kRemoteAmmoCount, false);
  if (!valid_remote_times(times, "远程兑换发弹量")) {
    return request;
  }
  request.delta = times;
  request.monotonic = true;
  // 每次 = 100 发；协议字段是累计次数，不是发数。
  request.target = std::min(kRemoteCountCap,
                            static_cast<int>(upstream.info1.remote_ammo_exchange_count) + times);
  return request;
}

ActionRequest make_remote_hp_exchange(const UpstreamState& upstream, int times) {
  ActionRequest request =
      make_base(DecisionActionKind::kRemoteHpExchange, ActionVerify::kRemoteHpCount, false);
  if (!valid_remote_times(times, "远程兑换血量")) {
    return request;
  }
  request.delta = times;
  request.monotonic = true;
  request.target = std::min(kRemoteCountCap,
                            static_cast<int>(upstream.info1.remote_health_exchange_count) + times);
  return request;
}

ActionRequest make_free_resurrect() {
  ActionRequest request = make_base(DecisionActionKind::kFreeResurrect, ActionVerify::kAlive, true);
  request.target = 1;
  request.delta = 1;
  return request;
}

ActionRequest make_instant_resurrect() {
  ActionRequest request =
      make_base(DecisionActionKind::kInstantResurrect, ActionVerify::kAlive, true);
  request.target = 1;
  request.delta = 1;
  return request;
}

bool verify_action(ActionVerify verify, int target, const UpstreamState& baseline,
                   const UpstreamState& now) {
  switch (verify) {
    case ActionVerify::kAmmoExchangedLocal:
      return now.info1.ammo_exchanged_local >= target;
    case ActionVerify::kRemoteAmmoCount:
      return now.info1.remote_ammo_exchange_count >= target;
    case ActionVerify::kRemoteHpCount:
      return now.info1.remote_health_exchange_count >= target;
    case ActionVerify::kAlive:
      return baseline.self_hp <= 0 && now.self_hp > 0;
    case ActionVerify::kNone:
    default:
      return false;
  }
}

}  // namespace sentry_decision
