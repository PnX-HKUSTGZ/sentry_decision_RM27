#include "sentry_decision_nodes/nav_nodes.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "sentry_decision_core/logging.hpp"
#include "sentry_decision_nodes/detail/node_helpers.hpp"

namespace sentry_decision {
namespace {

// 拆分逗号分隔的命名点列表，去掉空白；空项忽略。
std::vector<std::string> split_points(const std::string& text) {
  std::vector<std::string> points;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, ',')) {
    const auto begin = item.find_first_not_of(" \t");
    if (begin == std::string::npos) {
      continue;
    }
    const auto end = item.find_last_not_of(" \t");
    points.push_back(item.substr(begin, end - begin + 1));
  }
  return points;
}

void emit_goal(DecisionContext* context, const Point2D& point) {
  Intent intent;
  intent.field = IntentField::kNavGoal;
  intent.source = SourceId::kSkill;
  intent.priority = Priority::kTactical;
  intent.stamp = context->world.stamp;
  intent.value = point;
  context->emit(intent);
}

void reset_cursor(DecisionContext* context, const std::string& loop_id) {
  PatrolCursor& cursor = context->patrol[loop_id];
  cursor.index = 0;
  cursor.dwell_start.reset();
}

}  // namespace

// [EmitNavGoalFromPoint]
EmitNavGoalFromPoint::EmitNavGoalFromPoint(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config) {}

BT::PortsList EmitNavGoalFromPoint::providedPorts() {
  return {BT::InputPort<std::string>("point")};
}

BT::NodeStatus EmitNavGoalFromPoint::tick() {
  const auto point = getInput<std::string>("point");
  if (!point) {
    return BT::NodeStatus::FAILURE;
  }
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  const Point2D* resolved = context->config->find_point(point.value());
  if (resolved == nullptr) {
    SD_LOG_WARN("nodes", "EmitNavGoalFromPoint 缺少命名点: %s", point.value().c_str());
    return BT::NodeStatus::FAILURE;
  }
  emit_goal(context, *resolved);
  return BT::NodeStatus::SUCCESS;
}

// [PatrolLoop]
PatrolLoop::PatrolLoop(const std::string& name, const BT::NodeConfig& config)
    : BT::StatefulActionNode(name, config) {}

BT::PortsList PatrolLoop::providedPorts() {
  return {BT::InputPort<std::string>("points", ""), BT::InputPort<std::string>("dwell_key", ""),
          BT::InputPort<std::string>("loop_id", "")};
}

BT::NodeStatus PatrolLoop::onStart() {
  return onRunning();
}

BT::NodeStatus PatrolLoop::onRunning() {
  DecisionContext* context = context_from(config());
  if (context == nullptr || context->config == nullptr) {
    return BT::NodeStatus::FAILURE;
  }
  const auto points_text = getInput<std::string>("points");
  if (!points_text) {
    SD_LOG_WARN("nodes", "PatrolLoop 缺少 points 端口");
    return BT::NodeStatus::FAILURE;
  }
  const std::vector<std::string> points = split_points(points_text.value());
  if (points.empty()) {
    SD_LOG_WARN("nodes", "PatrolLoop 的 points 为空");
    return BT::NodeStatus::FAILURE;
  }
  const auto dwell_key = getInput<std::string>("dwell_key");
  if (!dwell_key) {
    SD_LOG_WARN("nodes", "PatrolLoop 缺少 dwell_key 端口");
    return BT::NodeStatus::FAILURE;
  }
  const auto dwell_s = context->config->number(dwell_key.value());
  if (!dwell_s.has_value() || dwell_s.value() < 0.0) {
    SD_LOG_WARN("nodes", "PatrolLoop 缺少或非法配置 key: %s", dwell_key.value().c_str());
    return BT::NodeStatus::FAILURE;
  }

  std::string loop_id = name();
  if (const auto id = getInput<std::string>("loop_id"); id && !id.value().empty()) {
    loop_id = id.value();
  }
  PatrolCursor& cursor = context->patrol[loop_id];
  if (cursor.index < 0 || static_cast<std::size_t>(cursor.index) >= points.size()) {
    cursor.index = 0;
    cursor.dwell_start.reset();
  }

  // 到点后停留 dwell_s；停留结束切下一个点（循环）。未到点则清空停留计时。
  const Duration dwell =
      Duration{static_cast<std::int64_t>(std::llround(dwell_s.value() * 1000.0))};
  if (context->world.nav.reached) {
    if (!cursor.dwell_start.has_value()) {
      cursor.dwell_start = context->world.stamp;
    }
    if (context->world.stamp - *cursor.dwell_start >= dwell) {
      cursor.index = (cursor.index + 1) % static_cast<int>(points.size());
      cursor.dwell_start.reset();
    }
  } else {
    cursor.dwell_start.reset();
  }

  const Point2D* point = context->config->find_point(points[cursor.index]);
  if (point == nullptr) {
    SD_LOG_WARN("nodes", "PatrolLoop 缺少命名点: %s", points[cursor.index].c_str());
    return BT::NodeStatus::FAILURE;
  }
  emit_goal(context, *point);
  return BT::NodeStatus::RUNNING;
}

void PatrolLoop::onHalted() {
  DecisionContext* context = context_from(config());
  if (context == nullptr) {
    return;
  }
  std::string loop_id = name();
  if (const auto id = getInput<std::string>("loop_id"); id && !id.value().empty()) {
    loop_id = id.value();
  }
  reset_cursor(context, loop_id);
}

void register_nav_nodes(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<EmitNavGoalFromPoint>("EmitNavGoalFromPoint");
  factory.registerNodeType<PatrolLoop>("PatrolLoop");
}

}  // namespace sentry_decision

BT_REGISTER_NODES(factory) {
  sentry_decision::register_nav_nodes(factory);
}
