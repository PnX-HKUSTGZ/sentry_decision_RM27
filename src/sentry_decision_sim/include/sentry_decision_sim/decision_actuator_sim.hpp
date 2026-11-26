#pragma once

#include <cstddef>
#include <vector>

#include "sentry_decision_core/io.hpp"
#include "sentry_decision_core/types.hpp"
#include "sentry_decision_sim/sim_world.hpp"

namespace sentry_decision_sim {

// 本地动作执行端模拟：接收决策动作，经过固定 tick 延迟后结算进 SimWorld。
// 没有回执——上位机只能通过 SimWorld 产生、referee_sim_node 发布的上行数据观察效果，
// 与真实链路一致。
class DecisionActuatorSim : public sentry_decision::DecisionSink {
 public:
  explicit DecisionActuatorSim(int latency_ticks = 2);

  // 绑定仿真世界后，动作到点时会按裁判规则结算（金币扣减、血量 / 发弹量增加）。
  // 未绑定（world == nullptr）时动作不产生任何效果。
  void bind_world(SimWorld* world, int max_hp = 0);

  // 接收一条待执行动作（DecisionSink）。
  void send_action(const sentry_decision::DecisionAction& action) override;

  // 记录当前任务状态；仿真执行端不消费，只供测试 / 面板观察。
  void send_task_state(sentry_decision::TaskState state) override;

  sentry_decision::TaskState task_state() const {
    return task_state_;
  }

  // 推进一个 tick：在途动作倒计时，到点结算。
  void update();

  void reset();

  std::size_t in_flight() const {
    return in_flight_.size();
  }

 private:
  struct InFlight {
    sentry_decision::DecisionAction action;
    int remaining = 0;
  };

  int latency_ticks_;
  std::vector<InFlight> in_flight_;
  SimWorld* world_ = nullptr;
  int max_hp_ = 0;
  sentry_decision::TaskState task_state_ = sentry_decision::TaskState::kIdle;
};

}  // namespace sentry_decision_sim
