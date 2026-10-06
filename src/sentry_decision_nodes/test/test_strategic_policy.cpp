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
  world.upstream.valid = true;
  world.upstream.self_hp = 400;
  world.upstream.self_ammo = 100;
  world.upstream.our_outpost_hp = 1500;
  world.upstream.enemy_outpost_hp = 1500;
  world.upstream.game_time_remaining = 420;
  world.upstream.game_status = GameStatus::kRunning;
  return world;
}

void test_priority() {
  const RuleBasedStrategicPolicy policy;
  // 每个用例用独立记忆，隔离撤退迟滞。
  const auto decide = [&policy](const WorldState& world) {
    StrategicMemory memory;
    return policy.decide(world, &memory);
  };

  // 规则 1：敌方前哨存活 -> 进攻。
  WorldState world = make_world();
  CHECK(decide(world).mode == TacticalMode::kAttack);
  CHECK(decide(world).stance == SentryStance::kAttack);

  world.upstream.self_hp = 50;
  CHECK(decide(world).mode == TacticalMode::kRetreat);
  CHECK(decide(world).stance == SentryStance::kMove);

  world.upstream.self_hp = 0;
  CHECK(decide(world).mode == TacticalMode::kRespawn);
  CHECK(decide(world).stance == SentryStance::kMove);

  world = make_world();
  world.upstream.self_ammo = 10;
  CHECK(decide(world).mode == TacticalMode::kHeal);

  // 我方前哨阵亡但敌方前哨存活：仍进攻（不再回堡垒）。
  world = make_world();
  world.upstream.our_outpost_hp = 0;
  CHECK(decide(world).mode == TacticalMode::kAttack);

  // 规则 2：敌方前哨被毁、我方前哨存活 -> 巡逻（高地循环由任务树区分）。
  world = make_world();
  world.upstream.enemy_outpost_hp = 0;
  CHECK(decide(world).mode == TacticalMode::kPatrol);

  // 规则 3：双方前哨皆毁，剩余 > 180s -> 后方巡逻。
  world = make_world();
  world.upstream.enemy_outpost_hp = 0;
  world.upstream.our_outpost_hp = 0;
  world.upstream.game_time_remaining = 420;
  CHECK(decide(world).mode == TacticalMode::kPatrol);

  // 规则 3：双方前哨皆毁，剩余 <= 180s -> 回堡垒防守。
  world.upstream.game_time_remaining = 100;
  CHECK(decide(world).mode == TacticalMode::kDefend);
  CHECK(decide(world).stance == SentryStance::kDefense);

  world = make_world();
  world.upstream.game_status = GameStatus::kPreparation;  // 未进入比赛中 -> 待机
  CHECK(decide(world).mode == TacticalMode::kIdle);

  world = make_world();
  world.upstream.valid = false;
  CHECK(decide(world).mode == TacticalMode::kUnknown);
  CHECK(decide(world).stance == SentryStance::kUnknown);
}

// 撤退迟滞：低于进入阈值进入，高于进入阈值但未到恢复线仍保持，到达恢复线才退出。
void test_retreat_hysteresis() {
  PolicyConfig config;
  config.numbers["nav.retreat_hp"] = 50;
  config.numbers["nav.recovery_hp"] = 200;
  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);
  StrategicMemory memory;

  WorldState world = make_world();
  world.upstream.self_hp = 40;
  CHECK(policy.decide(world, &memory).mode == TacticalMode::kRetreat);
  CHECK(memory.retreat_latched);

  world.upstream.self_hp = 120;  // 高于进入阈值但未到恢复线 -> 保持撤退
  CHECK(policy.decide(world, &memory).mode == TacticalMode::kRetreat);

  world.upstream.self_hp = 200;  // 到达恢复线 -> 退出撤退
  CHECK(policy.decide(world, &memory).mode == TacticalMode::kAttack);
  CHECK(!memory.retreat_latched);

  world.upstream.self_hp = 40;  // 再次低于进入阈值 -> 重新进入
  CHECK(policy.decide(world, &memory).mode == TacticalMode::kRetreat);
  // 阵亡清迟滞。
  world.upstream.self_hp = 0;
  CHECK(policy.decide(world, &memory).mode == TacticalMode::kRespawn);
  CHECK(!memory.retreat_latched);
}

void test_from_config() {
  PolicyConfig config;
  config.numbers["nav.retreat_hp"] = 120;
  config.numbers["nav.recovery_hp"] = 400;
  config.numbers["nav.low_ammo"] = 30;
  config.numbers["strategic.fort_after_remaining_s"] = 200;
  const RuleBasedStrategicPolicy policy = RuleBasedStrategicPolicy::from_config(config);
  const auto decide = [&policy](const WorldState& world) {
    StrategicMemory memory;
    return policy.decide(world, &memory);
  };

  WorldState world = make_world();
  world.upstream.self_hp = 100;  // <= 120 -> 撤退
  CHECK(decide(world).mode == TacticalMode::kRetreat);

  world = make_world();
  world.upstream.self_ammo = 20;  // <= 30 -> 补给
  CHECK(decide(world).mode == TacticalMode::kHeal);

  world = make_world();
  world.upstream.enemy_outpost_hp = 0;
  world.upstream.our_outpost_hp = 0;
  world.upstream.game_time_remaining = 250;  // > 200 -> 后方巡逻
  CHECK(decide(world).mode == TacticalMode::kPatrol);
  world.upstream.game_time_remaining = 150;  // <= 200 -> 防守
  CHECK(decide(world).mode == TacticalMode::kDefend);
}

}  // namespace

int main() {
  test_priority();
  test_retreat_hysteresis();
  test_from_config();
  if (g_failures == 0) {
    std::printf("all strategic policy tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
