#include <cstdint>
#include <cstdio>

#include "sentry_decision_io/decision_state_convert.hpp"

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

void test_decision_output_conversion() {
  DecisionOutput empty;
  const DecisionOutputMsg empty_msg = to_msg(empty);
  CHECK(!empty_msg.has_nav_goal);
  CHECK(!empty_msg.has_cmd_vel);

  DecisionOutput output;
  output.nav_goal = Point2D{1.5, -2.5, 0.75};
  output.cmd_vel = Twist{0.1, 0.2, 0.3};
  output.stance = SentryStance::kAttack;
  output.tactical_mode = TacticalMode::kRetreat;
  output.resource.ammo = 17;
  output.resource.hp = 3;
  output.resource.revive = true;

  const DecisionOutputMsg msg = to_msg(output);
  CHECK(msg.has_nav_goal);
  CHECK(msg.nav_goal.x == 1.5);
  CHECK(msg.nav_goal.y == -2.5);
  CHECK(msg.nav_goal_yaw == 0.75);
  CHECK(msg.has_cmd_vel);
  CHECK(msg.cmd_vel.linear.x == 0.1);
  CHECK(msg.cmd_vel.linear.y == 0.2);
  CHECK(msg.cmd_vel.angular.z == 0.3);
  CHECK(msg.stance == static_cast<std::uint8_t>(SentryStance::kAttack));
  CHECK(msg.tactical_mode == static_cast<std::uint8_t>(TacticalMode::kRetreat));
  CHECK(msg.resource_ammo == 17);
  CHECK(msg.resource_hp == 3);
  CHECK(msg.resource_revive);
}

void test_world_state_conversion() {
  WorldState world;
  world.referee.valid = true;
  world.referee.game_status = GameStatus::kRunning;
  world.referee.game_time_remaining = 200;
  world.referee.coins = 50;
  world.referee.self_hp = 400;
  world.referee.self_ammo = 100;
  world.referee.base_hp = 5000;
  world.referee.our_outpost_hp = 1500;
  world.referee.enemy_outpost_hp = 1200;
  world.referee.enemy_base_hp = 5000;
  world.referee.can_rebuild_outpost = true;

  world.self.valid = true;
  world.self.pose = Point2D{3.0, 4.0, 0.5};
  world.self.vx = 0.5;
  world.self.vy = -0.5;
  world.self.wz = 0.25;

  world.nav.valid = true;
  world.nav.current_goal = Point2D{9.0, 8.0, 0.0};
  world.nav.reached = true;

  world.enemy.valid = true;
  world.enemy.target_valid = true;
  world.enemy.position = Point2D{7.0, 6.0, 0.0};
  world.enemy.enemies.push_back(EnemyRobot{});
  world.allies.push_back(AllyRobot{});

  const WorldStateMsg msg = to_msg(world);
  CHECK(msg.referee_valid);
  CHECK(msg.game_status == static_cast<std::int32_t>(GameStatus::kRunning));
  CHECK(msg.game_time_remaining == 200);
  CHECK(msg.coins == 50);
  CHECK(msg.self_hp == 400);
  CHECK(msg.self_ammo == 100);
  CHECK(msg.base_hp == 5000);
  CHECK(msg.our_outpost_hp == 1500);
  CHECK(msg.enemy_outpost_hp == 1200);
  CHECK(msg.enemy_base_hp == 5000);
  CHECK(msg.can_rebuild_outpost);
  CHECK(msg.self_valid);
  CHECK(msg.pos_x == 3.0);
  CHECK(msg.pos_y == 4.0);
  CHECK(msg.yaw == 0.5);
  CHECK(msg.vx == 0.5);
  CHECK(msg.vy == -0.5);
  CHECK(msg.wz == 0.25);
  CHECK(msg.nav_valid);
  CHECK(msg.has_nav_goal);
  CHECK(msg.nav_goal_x == 9.0);
  CHECK(msg.nav_goal_y == 8.0);
  CHECK(msg.nav_reached);
  CHECK(msg.enemy_valid);
  CHECK(msg.enemy_target_valid);
  CHECK(msg.has_enemy_position);
  CHECK(msg.enemy_x == 7.0);
  CHECK(msg.enemy_y == 6.0);
  CHECK(msg.enemy_count == 1);
  CHECK(msg.ally_count == 1);
}

void test_fill_state() {
  WorldState world;
  world.referee.valid = true;
  world.self.valid = true;

  ArbiterResult result;
  result.output.nav_goal = Point2D{1.0, 2.0, 0.0};
  result.output.tactical_mode = TacticalMode::kPatrol;
  result.conflicts.push_back(Conflict{});
  result.warnings.push_back("non-owner submit");

  DecisionStateMsg msg;
  fill_state(&msg, world, result, 42);

  CHECK(msg.tick == 42);
  CHECK(msg.output.has_nav_goal);
  CHECK(msg.output.tactical_mode == static_cast<std::uint8_t>(TacticalMode::kPatrol));
  CHECK(msg.conflict_count == 1);
  CHECK(msg.warnings.size() == 1);
  CHECK(msg.referee_valid);
  CHECK(msg.self_valid);
  CHECK(!msg.nav_valid);
}

}  // namespace

int main() {
  test_decision_output_conversion();
  test_world_state_conversion();
  test_fill_state();
  if (g_failures == 0) {
    std::printf("all io decision_state_convert tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
