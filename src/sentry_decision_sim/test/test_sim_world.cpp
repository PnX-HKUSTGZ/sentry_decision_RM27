#include <cstdio>

#include "sentry_decision_sim/sim_world.hpp"

using namespace sentry_decision_sim;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// 本地兑换发弹量：10 金币/10 发，金币不足 / 数量非法时拒绝。
void test_exchange_ammo() {
  SimWorld world;
  world.coins = 100;
  world.self_ammo = 10;
  const ExchangeResult ok = exchange_ammo(&world, 50);
  CHECK(ok.accepted);
  CHECK(ok.amount == 50);
  CHECK(ok.coin_cost == 50);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  const ExchangeResult poor = exchange_ammo(&world, 80);
  CHECK(!poor.accepted);
  CHECK(world.self_ammo == 60);
  CHECK(world.coins == 50);

  const ExchangeResult zero = exchange_ammo(&world, 0);
  CHECK(!zero.accepted);
}

// 本地兑换血量：1 金币/1 点，受上限约束；满血或金币不足拒绝。
void test_exchange_hp() {
  SimWorld world;
  world.coins = 100;
  world.self_hp = 100;
  const ExchangeResult ok = exchange_hp(&world, 50, 400);
  CHECK(ok.accepted);
  CHECK(ok.amount == 50);
  CHECK(world.self_hp == 150);
  CHECK(world.coins == 50);

  world.coins = 1000;
  const ExchangeResult capped = exchange_hp(&world, 500, 400);
  CHECK(capped.accepted);
  CHECK(world.self_hp == 400);
  CHECK(capped.amount == 250);

  const ExchangeResult full = exchange_hp(&world, 50, 400);
  CHECK(!full.accepted);
  CHECK(world.self_hp == 400);
}

// 补给区回血：按上限血量比例，向上取整，且不超过上限。
void test_supply_heal() {
  SimWorld world;
  world.self_hp = 100;
  CHECK(supply_heal(&world, 400, 0.10) == 40);
  CHECK(world.self_hp == 140);
  CHECK(supply_heal(&world, 400, 0.25) == 100);
  CHECK(world.self_hp == 240);
  world.self_hp = 395;
  CHECK(supply_heal(&world, 400, 0.10) == 5);
  CHECK(world.self_hp == 400);
  CHECK(supply_heal(&world, 400, 0.10) == 0);
  // 上限 250 时 10% = 25。
  world.self_hp = 0;
  CHECK(supply_heal(&world, 250, 0.10) == 25);
}

}  // namespace

int main() {
  test_exchange_ammo();
  test_exchange_hp();
  test_supply_heal();
  if (g_failures == 0) {
    std::printf("all sim world tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
