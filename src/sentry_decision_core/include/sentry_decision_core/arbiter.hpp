#pragma once

#include <string>
#include <vector>

#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 同一字段上多个来源竞争时的胜者与败者记录。
struct Conflict {
  IntentField field;
  SourceId winner;
  std::vector<SourceId> losers;
};

struct ArbiterResult {
  DecisionOutput output;
  std::vector<Conflict> conflicts;
  std::vector<std::string> warnings;
};

// 意图仲裁器：把所有来源的 Intent 在字段级裁决成唯一的 DecisionOutput。
//
// 规则：
//   1. 每个字段有 owner 白名单，非白名单来源提交会被记录为告警；
//   2. 同一字段的候选按 优先级 > 时间戳（新者优先）> 来源枚举 排序，取最大者；
//   3. lease.count() == 0 表示不自动过期，否则超过 stamp + lease 即失效；
//   4. 相同 (source, field) 的旧意图会被新提交替换。
//
// 该组件是纯逻辑，不依赖 ROS，可在宿主机上直接单测。
class IntentArbiter {
 public:
  IntentArbiter() = default;

  // 提交一条意图；若已存在相同 (source, field)，则替换。
  void submit(const Intent& intent);

  // 撤销某来源的全部意图（例如 tick 开始时清空策略来源）。
  void clear_source(SourceId source);
  void clear_all();

  // 在给定时刻做一次字段级裁决。
  ArbiterResult resolve(TimePoint now) const;

  // 字段的 owner 与允许的覆盖者。
  static bool is_owner(IntentField field, SourceId source);
  static bool is_allowed(IntentField field, SourceId source);

 private:
  static bool is_valid(const Intent& intent, TimePoint now);

  std::vector<Intent> intents_;
};

}  // namespace sentry_decision
