#include <cstdio>

#include "sentry_decision_nodes/rule_based_strategic_policy.hpp"

using namespace sentry_decision;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

WorldState make_world() {
  WorldState world;
  world.referee.valid = true;
  world.referee.self_hp = 400;
  world.referee.self_ammo = 100;
  world.referee.our_outpost_hp = 1500;
  world.referee.enemy_outpost_hp = 1500;
  world.referee.game_time_remaining = 420;
  world.referee.game_status = GameStatus::kRunning;
  return world;
}

void test_priority() {
  const RuleBasedStrategicPolicy policy;

  // 规则 1：敌方前哨存活 -> 进攻。
  WorldState world = make_world();
  CHECK(policy.decide(world).mode == TacticalMode::kAttack);
  CHECK(policy.decide(world).stance == SentryStance::kAttack);

  world.referee.self_hp = 50;
  CHECK(policy.decide(world).mode == TacticalMode::kRetreat);
  CHECK(policy.decide(world).stance == SentryStance::kMove);

  world.referee.self_hp = 0;
  CHECK(policy.decide(world).mode == TacticalMode::kRespawn);
  CHECK(policy.decide(world).stance == SentryStance::kMove);

  world = make_world();
  world.referee.self_ammo = 10;
  CHECK(policy.decide(world).mode == TacticalMode::kHeal);

  // 我方前哨阵亡但敌方前哨存活：仍进攻（不再回堡垒）。
  world = make_world();
  world.referee.our_outpost_hp = 0;
  CHECK(policy.decide(world).mode == TacticalMode::kAttack);

  // 规则 2：敌方前哨被毁、我方前哨存活 -> 巡逻（高地循环由任务树区分）。
  world = make_world();
  world.referee.enemy_outpost_hp = 0;
  CHECK(policy.decide(world).mode == TacticalMode::kPatrol);

  // 规则 3：双方前哨皆毁，剩余 > 180s -> 后方巡逻。
  world = make_world();
  world.referee.enemy_outpost_hp = 0;
  world.referee.our_outpost_hp = 0;
  world.referee.game_time_remaining = 420;
  CHECK(policy.decide(world).mode == TacticalMode::kPatrol);

  // 规则 3：双方前哨皆毁，剩余 <= 180s -> 回堡垒防守。
  world.referee.game_time_remaining = 100;
  CHECK(policy.decide(world).mode == TacticalMode::kDefend);
  CHECK(policy.decide(world).stance == SentryStance::kDefense);

  world = make_world();
  world.referee.game_status = GameStatus::kPreparation;  // 未进入比赛中 -> 待机
  CHECK(policy.decide(world).mode == TacticalMode::kIdle);
  world.referee.game_status = GameStatus::kRunning;

  world = make_world();
  world.referee.valid = false;
  CHECK(policy.decide(world).mode == TacticalMode::kUnknown);
  CHECK(policy.decide(world).stance == SentryStance::kUnknown);
}

void test_from_config() {
  PolicyConfig config;
  config.numbers["nav.retreat_hp"] = 120;
  config.numbers["nav.low_ammo"] = 30;
  config.numbers["strategic.fort_after_remaining_s"] = 200;
  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);

  WorldState world = make_world();
  world.referee.self_hp = 100;  // <= 120 -> 撤退
  CHECK(policy.decide(world).mode == TacticalMode::kRetreat);

  world = make_world();
  world.referee.self_ammo = 20;  // <= 30 -> 补给
  CHECK(policy.decide(world).mode == TacticalMode::kHeal);

  world = make_world();
  world.referee.enemy_outpost_hp = 0;
  world.referee.our_outpost_hp = 0;
  world.referee.game_time_remaining = 250;  // > 200 -> 后方巡逻
  CHECK(policy.decide(world).mode == TacticalMode::kPatrol);
  world.referee.game_time_remaining = 150;  // <= 200 -> 防守
  CHECK(policy.decide(world).mode == TacticalMode::kDefend);
}

}  // namespace

int main() {
  test_priority();
  test_from_config();
  if (g_failures == 0) {
    std::printf("all strategic policy tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
