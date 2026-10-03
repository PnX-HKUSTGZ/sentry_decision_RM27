#pragma once

#include <optional>

#include "sentry_decision_core/context.hpp"
#include "sentry_decision_core/types.hpp"

namespace sentry_decision {

// 战术层覆盖：行为树之外对期望 TacticalMode 的人工覆盖（调试 / 测试）。
// 不进入仲裁优先级，也不修改世界状态；在战略层求值之后写入 context.strategy。
class TacticalOverride {
 public:
  // 设置覆盖；lease 为 0 表示不过期。
  void set(TacticalMode mode, Duration lease, TimePoint now);
  void clear();
  // 当前是否有生效的覆盖。
  bool active(TimePoint now) const;
  // 生效时返回覆盖模式，否则返回 nullopt。
  std::optional<TacticalMode> mode(TimePoint now) const;

 private:
  TacticalMode mode_ = TacticalMode::kUnknown;
  TimePoint stamp_{};
  Duration lease_{0};
  bool set_ = false;
};

// 在 apply_strategy 之后调用：有生效覆盖时改写 context.strategy.mode，
// 并同步已提交的 kTacticalMode 意图，使仲裁输出与任务树一致。
void apply_tactical_override(const TacticalOverride& override_value, TimePoint now,
                             DecisionContext* context);

}  // namespace sentry_decision