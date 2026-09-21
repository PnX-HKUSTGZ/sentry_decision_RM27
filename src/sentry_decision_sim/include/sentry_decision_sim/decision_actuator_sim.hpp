#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sentry_decision_core/io.hpp"
#include "sentry_decision_core/types.hpp"

namespace sentry_decision_sim {

// 本地动作执行端模拟：接收决策动作，经过固定 tick 延迟后产生成功回执。
// 用于在无 MCU / 无下位机协议时，离线驱动 ActionDispatcher 的 ack 闭环。
class DecisionActuatorSim : public sentry_decision::DecisionSink {
 public:
  explicit DecisionActuatorSim(int ack_latency_ticks = 2);

  // 接收一条待执行动作（DecisionSink）。
  void send_action(const sentry_decision::DecisionAction& action) override;

  // 推进一个 tick：在途动作倒计时，到点产生成功回执。
  void update();

  // 取出已产生的回执。
  std::vector<sentry_decision::ActionAck> take_acks();

  void reset();

  std::size_t in_flight() const {
    return in_flight_.size();
  }

 private:
  struct InFlight {
    std::uint32_t request_id = 0;
    int remaining = 0;
  };

  int ack_latency_ticks_;
  std::vector<InFlight> in_flight_;
  std::vector<sentry_decision::ActionAck> acks_;
};

}  // namespace sentry_decision_sim
