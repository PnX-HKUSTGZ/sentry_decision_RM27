#pragma once

#include "sentry_decision_core/types.hpp"
#include "sentry_decision_core/world_state.hpp"

namespace sentry_decision {

// 动作工厂：把“行为树想做什么”翻译成协议目标值 + 生效判据。
// 协议字段、累计值折算、最小兑换单位都集中在这里；dispatcher 不感知 kind 的语义。
//
// 所有函数的 delta / times 都是「本次增量」，target 由工厂按当前裁判累计值计算。
// 行为树节点每 tick 调用一次，target 可能随累计值变化；dispatcher 在一段请求
// 开始时取一次 target，并在该段请求持续期间忽略后续变化。

constexpr int kAmmoExchangeUnit = 10;    // 2027 表 5-8：17mm 增益点最小兑换单位
constexpr int kAmmoExchangeCap = 1000;   // 2027 表 5-7：17mm 兑换上限 1000 发/队
constexpr int kRemoteCountCap = 15;      // 0x020D 远程兑换次数 4 bit

ActionRequest make_ammo_exchange(const UpstreamState& upstream, int delta,
                                 Duration retry_interval, int max_attempts);

// times 是“次数”，每次 100 发（2027 表 5-9）；协议字段是累计次数。
ActionRequest make_remote_ammo_exchange(const UpstreamState& upstream, int times,
                                        Duration retry_interval, int max_attempts);

ActionRequest make_remote_hp_exchange(const UpstreamState& upstream, int times,
                                      Duration retry_interval, int max_attempts);

ActionRequest make_free_resurrect(Duration retry_interval, int max_attempts);

ActionRequest make_instant_resurrect(Duration retry_interval, int max_attempts);

// 生效判据（方案 A：枚举 + 纯函数）。
bool verify_action(ActionVerify verify, int target, const UpstreamState& baseline,
                   const UpstreamState& now);

}  // namespace sentry_decision
