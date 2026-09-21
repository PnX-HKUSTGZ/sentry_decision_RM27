#include <cstdio>
#include <memory>
#include <string>

#include "behaviortree_cpp/action_node.h"
#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_viz/tree_state.hpp"

using namespace sentry_decision_viz;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// 永远 RUNNING，用来制造 active path。
class HoldForever : public BT::StatefulActionNode {
 public:
  HoldForever(const std::string& name, const BT::NodeConfig& config)
      : BT::StatefulActionNode(name, config) {}

  static BT::PortsList providedPorts() {
    return {};
  }
  BT::NodeStatus onStart() override {
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    return BT::NodeStatus::RUNNING;
  }
  void onHalted() override {}
};

class SucceedOnce : public BT::ActionNodeBase {
 public:
  SucceedOnce(const std::string& name, const BT::NodeConfig& config)
      : BT::ActionNodeBase(name, config) {}

  static BT::PortsList providedPorts() {
    return {};
  }
  BT::NodeStatus tick() override {
    return BT::NodeStatus::SUCCESS;
  }
  void halt() override {}
};

std::unique_ptr<BT::Tree> make_tree(BT::BehaviorTreeFactory& factory) {
  factory.registerNodeType<HoldForever>("HoldForever");
  factory.registerNodeType<SucceedOnce>("SucceedOnce");
  const std::string xml = R"(
<root BTCPP_format="4">
  <BehaviorTree ID="Main">
    <Sequence name="seq">
      <HoldForever name="hold"/>
      <SucceedOnce name="ok"/>
    </Sequence>
  </BehaviorTree>
</root>
)";
  return std::make_unique<BT::Tree>(factory.createTreeFromText(xml));
}

// BT.CPP 的 fullPath 为节点实例名（子树内会带实例前缀）；比较最后一段即可。
bool is_node(const std::string& path, const std::string& name) {
  const std::size_t pos = path.find_last_of('/');
  return (pos == std::string::npos ? path : path.substr(pos + 1)) == name;
}

const sentry_decision_msgs::msg::TreeNodeStatus* find_node(
    const sentry_decision_msgs::msg::TreeStatus& status, const std::string& instance_name) {
  for (const auto& node : status.nodes) {
    if (node.instance_name == instance_name) {
      return &node;
    }
  }
  return nullptr;
}

void test_node_states_and_active_path() {
  BT::BehaviorTreeFactory factory;
  std::unique_ptr<BT::Tree> tree = make_tree(factory);
  tree->tickOnce();

  const sentry_decision_msgs::msg::TreeStatus status = collect_tree_status(*tree, 7, 1.5);
  CHECK(status.tick == 7);
  CHECK(status.tick_ms > 1.49 && status.tick_ms < 1.51);
  CHECK(status.nodes.size() == 3);

  const auto* hold = find_node(status, "hold");
  const auto* ok = find_node(status, "ok");
  CHECK(hold != nullptr);
  CHECK(ok != nullptr);
  if (hold != nullptr) {
    CHECK(hold->registration_name == "HoldForever");
    CHECK(hold->status == static_cast<std::uint8_t>(BT::NodeStatus::RUNNING));
  }
  // Sequence 的第一个孩子 RUNNING，第二个孩子本拍未执行，保持 IDLE。
  if (ok != nullptr) {
    CHECK(ok->status == static_cast<std::uint8_t>(BT::NodeStatus::IDLE));
  }

  // active path：RUNNING 的 hold 与其父 seq 在，未执行的 ok 不在。
  bool has_hold = false;
  bool has_seq = false;
  bool has_ok = false;
  for (const auto& path : status.active_path) {
    has_hold = has_hold || is_node(path, "hold");
    has_seq = has_seq || is_node(path, "seq");
    has_ok = has_ok || is_node(path, "ok");
  }
  CHECK(has_hold);
  CHECK(has_seq);
  CHECK(!has_ok);
  CHECK(status.active_path.size() == 2);
}

void test_all_success_has_empty_active_path() {
  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<SucceedOnce>("SucceedOnce");
  const std::string xml = R"(
<root BTCPP_format="4">
  <BehaviorTree ID="Main">
    <SucceedOnce name="ok"/>
  </BehaviorTree>
</root>
)";
  BT::Tree tree = factory.createTreeFromText(xml);
  tree.tickOnce();

  const sentry_decision_msgs::msg::TreeStatus status = collect_tree_status(tree, 0, 0.0);
  CHECK(status.nodes.size() == 1);
  CHECK(status.active_path.empty());
}

}  // namespace

int main() {
  test_node_states_and_active_path();
  test_all_success_has_empty_active_path();

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_tree_state passed\n");
  return 0;
}
