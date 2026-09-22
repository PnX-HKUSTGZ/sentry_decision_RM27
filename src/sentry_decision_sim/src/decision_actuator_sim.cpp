#include "sentry_decision_sim/decision_actuator_sim.hpp"

namespace sentry_decision_sim {

DecisionActuatorSim::DecisionActuatorSim(int ack_latency_ticks)
    : ack_latency_ticks_(ack_latency_ticks < 0 ? 0 : ack_latency_ticks) {}

void DecisionActuatorSim::bind_world(SimWorld* world, int max_hp) {
  world_ = world;
  max_hp_ = max_hp;
}

void DecisionActuatorSim::send_action(const sentry_decision::DecisionAction& action) {
  in_flight_.push_back(InFlight{action, ack_latency_ticks_});
}

void DecisionActuatorSim::update() {
  std::vector<InFlight> still_in_flight;
  still_in_flight.reserve(in_flight_.size());
  for (auto& item : in_flight_) {
    if (item.remaining > 0) {
      --item.remaining;
    }
    if (item.remaining > 0) {
      still_in_flight.push_back(item);
      continue;
    }
    acks_.push_back(resolve(item.action));
  }
  in_flight_.swap(still_in_flight);
}

// 兑换类动作在仿真执行端结算：扣金币、加血量/发弹量，失败时给出拒绝回执。
// 未绑定世界时保持历史上「无条件成功」的语义，供纯 ack 闭环测试使用。
sentry_decision::ActionAck DecisionActuatorSim::resolve(
    const sentry_decision::DecisionAction& action) {
  using K = sentry_decision::DecisionActionKind;
  sentry_decision::ActionAck ack;
  ack.request_id = action.request_id;
  if (world_ == nullptr) {
    ack.accepted = true;
    ack.code = 0;
    return ack;
  }
  switch (action.kind) {
    case K::kAmmoExchange: {
      const ExchangeResult result = exchange_ammo(world_, action.value);
      ack.accepted = result.accepted;
      ack.code = result.accepted ? 0 : 2;
      ack.detail = result.detail;
      break;
    }
    case K::kHpExchange: {
      const ExchangeResult result = exchange_hp(world_, action.value, max_hp_);
      ack.accepted = result.accepted;
      ack.code = result.accepted ? 0 : 2;
      ack.detail = result.detail;
      break;
    }
    case K::kInstantResurrect:
      world_->self_hp = max_hp_;
      ack.accepted = true;
      ack.detail = "立即复活：血量回满";
      break;
    default:
      // 免费复活 / 远程兑换等由真实协议决定，仿真只回执成功。
      ack.accepted = true;
      break;
  }
  return ack;
}

std::vector<sentry_decision::ActionAck> DecisionActuatorSim::take_acks() {
  std::vector<sentry_decision::ActionAck> acks;
  acks.swap(acks_);
  return acks;
}

void DecisionActuatorSim::reset() {
  in_flight_.clear();
  acks_.clear();
}

}  // namespace sentry_decision_sim
