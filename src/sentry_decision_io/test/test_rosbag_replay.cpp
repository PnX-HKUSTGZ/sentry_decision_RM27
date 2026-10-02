#include <cstdio>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>
#include <rosbag2_cpp/writer.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/u_int16.hpp>
#include <string>
#include <variant>

#include "sentry_decision_io/rosbag_replay.hpp"
#include "sentry_decision_msgs/msg/intervention_event.hpp"

using namespace sentry_decision;
using namespace sentry_decision_io;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

void create_topic(rosbag2_cpp::Writer& writer, const std::string& name, const std::string& type) {
  rosbag2_storage::TopicMetadata metadata;
  metadata.name = name;
  metadata.type = type;
  metadata.serialization_format = "cdr";
  writer.create_topic(metadata);
}

void test_round_trip() {
  const std::string dir = "/tmp/sentry_decision_replay_test_bag";
  std::filesystem::remove_all(dir);

  {
    rosbag2_cpp::Writer writer;
    writer.open(dir);
    create_topic(writer, "/ifhealth", "std_msgs/msg/UInt16");
    create_topic(writer, "/remain_ammo", "std_msgs/msg/UInt16");
    create_topic(writer, "/can_rebuild_outpost", "std_msgs/msg/Bool");
    create_topic(writer, "/decision/intervention", "sentry_decision_msgs/msg/InterventionEvent");

    std_msgs::msg::UInt16 hp;
    hp.data = 400;
    writer.write(hp, "/ifhealth", rclcpp::Time(0, 0));

    std_msgs::msg::UInt16 ammo;
    ammo.data = 100;
    writer.write(ammo, "/remain_ammo", rclcpp::Time(0, 100000000u));

    std_msgs::msg::Bool rebuild;
    rebuild.data = true;
    writer.write(rebuild, "/can_rebuild_outpost", rclcpp::Time(0, 200000000u));

    sentry_decision_msgs::msg::InterventionEvent event;
    event.kind = sentry_decision_msgs::msg::InterventionEvent::KIND_INTENT;
    event.field = 0;
    event.value = "[1.0, 2.0]";
    event.lease_sec = 3.0;
    event.reason = "test";
    writer.write(event, "/decision/intervention", rclcpp::Time(0, 300000000u));
    writer.close();
  }

  const ReplayData data = load_replay_data(dir);
  CHECK(data.referee.size() == 3);
  if (data.referee.size() == 3) {
    CHECK(data.referee[0].value.self_hp == 400);
    CHECK(data.referee[1].value.self_ammo == 100);
    CHECK(data.referee[2].value.can_rebuild_outpost);
    CHECK(data.referee[2].value.self_hp == 400);
    CHECK(data.referee[0].at == Duration{0});
    CHECK(data.referee[1].at == Duration{100});
    CHECK(data.referee[2].at == Duration{200});
    CHECK(data.referee[2].value.valid);
  }
  CHECK(data.odometry.empty());

  CHECK(data.interventions.size() == 1);
  if (data.interventions.size() == 1) {
    const InterventionCommand& command = data.interventions[0].value;
    CHECK(data.interventions[0].at == Duration{300});
    CHECK(command.kind == InterventionCommand::Kind::kIntent);
    CHECK(command.intent.field == IntentField::kNavGoal);
    CHECK(std::get<Point2D>(command.intent.value).x == 1.0);
    CHECK(std::get<Point2D>(command.intent.value).y == 2.0);
    CHECK(command.intent.lease == Duration{3000});
    CHECK(command.reason == "test");
  }

  std::filesystem::remove_all(dir);
}

}  // namespace

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  test_round_trip();
  rclcpp::shutdown();
  if (g_failures == 0) {
    std::printf("all io rosbag_replay tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
