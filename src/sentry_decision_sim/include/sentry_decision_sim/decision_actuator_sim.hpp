#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sentry_decision_core/io.hpp"
#include "sentry_decision_core/types.hpp"
#include "sentry_decision_sim/sim_world.hpp"

namespace sentry_decision_sim {

// 本地动作执行端模拟：接收决策动作，经过固定 tick 延迟后产生成功回执。
// 用于在无 MCU / 无下位机协议时，离线驱动 ActionDispatcher 的 ack 闭环。
class DecisionActuatorSim : public sentry_decision::DecisionSink {
 public:
  explicit DecisionActuatorSim(int ack_latency_ticks = 2);

  // 绑定仿真世界后，回执产生时把兑换类动作结算进世界（金币扣减、血量/发弹量增加）；
  // max_hp 为血量上限。未绑定（world == nullptr）时退化为「无条件成功回执」。
  void bind_world(SimWorld* world, int max_hp = 0);

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
  // 动作到点后结算，返回回执（可带拒绝原因）。
  sentry_decision::ActionAck resolve(const sentry_decision::DecisionAction& action);

  struct InFlight {
    sentry_decision::DecisionAction action;
    int remaining = 0;
  };

  int ack_latency_ticks_;
  std::vector<InFlight> in_flight_;
  std::vector<sentry_decision::ActionAck> acks_;
  SimWorld* world_ = nullptr;
  int max_hp_ = 0;
};

}  // namespace sentry_decision_sim
