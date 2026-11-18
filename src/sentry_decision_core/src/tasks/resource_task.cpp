#include "sentry_decision_core/tasks/resource_task.hpp"

#include <string_view>
#include <variant>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr Duration kExchangeWindow{5000};
constexpr int kAmmoExchangeCap = 1000;
constexpr int kAmmoExchangeUnit = 10;

std::string point_name(std::string_view name) {
  return std::string(name);
}

}  // namespace

bool ResourceTask::can_start(const WorldState& world) const {
  return world.self.valid && world.upstream.valid;
}

void ResourceTask::start(const TaskCall& call, TimePoint now) {
  started_ = true;
  invalid_ = false;
  sent_ = false;
  invalid_reason_.clear();
  baseline_ = 0;
  target_ = 0;
  request_ = ActionRequest{};
  sent_at_ = now;

  const auto* params = std::get_if<ResourceParams>(&call.params);
  if (params == nullptr) {
    invalid_ = true;
    invalid_reason_ = "Resource 参数类型不匹配";
    return;
  }
  params_ = *params;

  const bool base = params_.exchange_point == point_name(points2027::kOwnBaseSupply);
  const bool outpost = params_.exchange_point == point_name(points2027::kOwnOutpostBuff);
  if (!base && !outpost) {
    invalid_ = true;
    invalid_reason_ = "Resource 点必须是 own_base_supply 或 own_outpost_buff";
    return;
  }
  if (params_.amount <= 0 || params_.amount % kAmmoExchangeUnit != 0) {
    invalid_ = true;
    invalid_reason_ = "Resource amount 必须是 10 的正整数倍";
  }
}

TaskOutput ResourceTask::tick(const WorldState& world, TimePoint now) {
  TaskOutput out;
  out.task_state = TaskState::kResource;
  out.phase = "exchange";
  if (!started_ || invalid_) {
    out.status = TaskStatus::kFailure;
    out.detail = invalid_reason_.empty() ? "Resource 未启动" : invalid_reason_;
    return out;
  }

  if (!world.upstream.valid) {
    out.status = TaskStatus::kFailure;
    out.detail = "裁判数据无效";
    return out;
  }

  const bool base = params_.exchange_point == point_name(points2027::kOwnBaseSupply);
  const bool outpost = params_.exchange_point == point_name(points2027::kOwnOutpostBuff);

  if (!sent_) {
    if (base && !world.upstream.event.supply_zone_occupied) {
      out.status = TaskStatus::kFailure;
      out.detail = "不在己方基地补给区";
      return out;
    }
    if (outpost) {
      if (world.upstream.our_outpost_control != OutpostControl::kOwnFull) {
        out.status = TaskStatus::kFailure;
        out.detail = "己方前哨站未完全控制";
        return out;
      }
      if (world.upstream.event.our_outpost_buff_status == 0) {
        out.status = TaskStatus::kFailure;
        out.detail = "不在己方前哨站增益点";
        return out;
      }
    }

    baseline_ = world.upstream.info1.ammo_exchanged_local;
    if (baseline_ + params_.amount > kAmmoExchangeCap) {
      out.status = TaskStatus::kFailure;
      out.detail = "兑换后超过 1000 发上限";
      return out;
    }
    if (world.upstream.coins < params_.amount) {
      out.status = TaskStatus::kFailure;
      out.detail = "金币不足";
      return out;
    }

    target_ = baseline_ + params_.amount;
    request_ = ActionRequest{};
    request_.kind = DecisionActionKind::kAmmoExchange;
    request_.target = target_;
    request_.delta = params_.amount;
    request_.monotonic = true;
    request_.verify = ActionVerify::kAmmoExchangedLocal;
    out.action = request_;
    sent_ = true;
    sent_at_ = now;
    out.metrics["target"] = static_cast<double>(target_);
    out.metrics["baseline"] = static_cast<double>(baseline_);
    out.status = TaskStatus::kRunning;
    return out;
  }

  // 继续提交同一请求：dispatcher 负责去重；若上一拍因上游无效没有真正发出，
  // 这里还能把它保留下来，而不是在下一拍被清理掉。
  out.action = request_;
  out.metrics["current"] = static_cast<double>(world.upstream.info1.ammo_exchanged_local);
  out.metrics["target"] = static_cast<double>(target_);
  if (world.upstream.info1.ammo_exchanged_local >= target_) {
    out.status = TaskStatus::kSuccess;
    out.progress = 1.0f;
    out.detail = "兑换已到账";
    return out;
  }
  if (now - sent_at_ >= kExchangeWindow) {
    out.status = TaskStatus::kFailure;
    out.detail = "兑换 5 s 内未到账";
    return out;
  }

  out.status = TaskStatus::kRunning;
  return out;
}

void ResourceTask::abort() {
  started_ = false;
  sent_ = false;
}

}  // namespace sentry_decision
