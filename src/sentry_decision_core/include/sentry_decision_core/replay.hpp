#pragma once

#include <cstdint>
#include <vector>

#include "sentry_decision_core/io.hpp"

namespace sentry_decision {

// 带仿真时间戳的一条输入记录。
template <typename T>
struct ReplayRecord {
  Duration at{};  // 相对回放起点的仿真时间
  T value{};
};

// 离线回放数据：每个输入通道按 at 升序排列。
// rosbag 读取由 io 适配器负责，core 只消费这份与 ROS 解耦的数据。
struct ReplayData {
  std::vector<ReplayRecord<RefereeState>> referee;
  std::vector<ReplayRecord<SelfState>> odometry;
  std::vector<ReplayRecord<NavState>> navigation;
};

// 确定性输入回放：以固定步长推进仿真时间，返回当前时刻生效的记录。
//
// 时间语义：外部传入固定 epoch，记录时间戳为 epoch + at；
// 调用方用 stamp() 作为 WorldModel::snapshot 的 now，保证新鲜度判定与回放一致。
// 每次 step 的步长固定、遍历顺序固定，因此同一份数据两次回放输出完全一致。
class ReplaySource : public RefereeSource, public OdometrySource, public NavigationSink {
 public:
  explicit ReplaySource(ReplayData data, TimePoint epoch = TimePoint{});

  // 推进一个步长并 tick 计数加一；返回推进后的仿真时间。
  Duration step(Duration period);

  Duration now() const {
    return now_;
  }
  TimePoint stamp() const {
    return epoch_ + now_;
  }
  std::uint32_t tick() const {
    return tick_;
  }
  bool finished() const {
    return now_ >= last_at_;
  }

  bool referee(RefereeState* out) const override;
  bool odometry(SelfState* out) const override;
  NavState status() const override;

  // 回放期间不真实下发，只计数，便于回归时断言决策产出的目标序列。
  void send_goal(const Point2D& goal) override;
  void cancel_goal() override;
  std::uint32_t sent_goals() const {
    return sent_goals_;
  }
  std::uint32_t canceled_goals() const {
    return canceled_goals_;
  }
  Point2D last_goal() const {
    return last_goal_;
  }

 private:
  // 返回 at <= now 的最后一条记录；无则 nullptr。records 需按 at 升序。
  template <typename T>
  static const ReplayRecord<T>* latest(const std::vector<ReplayRecord<T>>& records, Duration now) {
    const ReplayRecord<T>* found = nullptr;
    for (const auto& record : records) {
      if (record.at > now) {
        break;
      }
      found = &record;
    }
    return found;
  }

  ReplayData data_;
  TimePoint epoch_{};
  Duration now_{};
  Duration last_at_{};
  std::uint32_t tick_ = 0;
  std::uint32_t sent_goals_ = 0;
  std::uint32_t canceled_goals_ = 0;
  Point2D last_goal_{};
};

}  // namespace sentry_decision
