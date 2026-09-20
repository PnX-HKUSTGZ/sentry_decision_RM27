#include "sentry_decision_io/decision_state_convert.hpp"

namespace sentry_decision_io {

DecisionOutputMsg to_msg(const sentry_decision::DecisionOutput& output) {
  DecisionOutputMsg msg;
  if (output.nav_goal.has_value()) {
    msg.has_nav_goal = true;
    msg.nav_goal.x = output.nav_goal->x;
    msg.nav_goal.y = output.nav_goal->y;
    msg.nav_goal.z = 0.0;
    msg.nav_goal_yaw = output.nav_goal->yaw;
  }
  if (output.cmd_vel.has_value()) {
    msg.has_cmd_vel = true;
    msg.cmd_vel.linear.x = output.cmd_vel->vx;
    msg.cmd_vel.linear.y = output.cmd_vel->vy;
    msg.cmd_vel.angular.z = output.cmd_vel->wz;
  }
  msg.tactical_mode = static_cast<std::uint8_t>(output.tactical_mode);
  msg.resource_ammo = static_cast<std::uint16_t>(output.resource.ammo);
  msg.resource_hp = static_cast<std::uint16_t>(output.resource.hp);
  msg.resource_revive = output.resource.revive;
  return msg;
}

WorldStateMsg to_msg(const sentry_decision::WorldState& world) {
  WorldStateMsg msg;
  const auto& referee = world.referee;
  msg.game_status = static_cast<std::int32_t>(referee.game_status);
  msg.game_time_remaining = referee.game_time_remaining;
  msg.coins = referee.coins;
  msg.self_hp = referee.self_hp;
  msg.self_ammo = referee.self_ammo;
  msg.base_hp = referee.base_hp;
  msg.our_outpost_hp = referee.our_outpost_hp;
  msg.enemy_outpost_hp = referee.enemy_outpost_hp;
  msg.enemy_base_hp = referee.enemy_base_hp;
  msg.can_rebuild_outpost = referee.can_rebuild_outpost;
  msg.referee_valid = referee.valid;

  msg.pos_x = world.self.pose.x;
  msg.pos_y = world.self.pose.y;
  msg.yaw = world.self.pose.yaw;
  msg.vx = world.self.vx;
  msg.vy = world.self.vy;
  msg.wz = world.self.wz;
  msg.self_valid = world.self.valid;

  msg.nav_valid = world.nav.valid;
  if (world.nav.current_goal.has_value()) {
    msg.has_nav_goal = true;
    msg.nav_goal_x = world.nav.current_goal->x;
    msg.nav_goal_y = world.nav.current_goal->y;
  }
  msg.nav_reached = world.nav.reached;
  msg.nav_failed = world.nav.failed;

  msg.enemy_valid = world.enemy.valid;
  msg.enemy_target_valid = world.enemy.target_valid;
  if (world.enemy.position.has_value()) {
    msg.has_enemy_position = true;
    msg.enemy_x = world.enemy.position->x;
    msg.enemy_y = world.enemy.position->y;
  }
  msg.enemy_count = static_cast<std::uint8_t>(world.enemy.enemies.size());
  msg.ally_count = static_cast<std::uint8_t>(world.allies.size());
  return msg;
}

void fill_state(DecisionStateMsg* msg, const sentry_decision::WorldState& world,
                const sentry_decision::ArbiterResult& result, std::uint32_t tick) {
  msg->tick = tick;
  msg->output = to_msg(result.output);
  msg->conflict_count = static_cast<std::uint32_t>(result.conflicts.size());
  msg->warnings = result.warnings;
  msg->referee_valid = world.referee.valid;
  msg->self_valid = world.self.valid;
  msg->nav_valid = world.nav.valid;
}

}  // namespace sentry_decision_io
