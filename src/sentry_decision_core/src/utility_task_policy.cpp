#include "sentry_decision_core/utility_task_policy.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

#include "sentry_decision_core/points_2027.hpp"

namespace sentry_decision {
namespace {

constexpr double kOutpostMaxHp = 1000.0;
constexpr Duration kNavigateTimeout{30000};

std::string point_name(std::string_view name) {
  return std::string(name);
}

int outpost_hp(const WorldState& world, Side side) {
  return side == Side::kOwn ? world.upstream.our_outpost_hp : world.upstream.enemy_outpost_hp;
}

OutpostControl outpost_control(const WorldState& world, Side side) {
  return side == Side::kOwn ? world.upstream.our_outpost_control
                            : world.upstream.enemy_outpost_control;
}

OutpostOwner outpost_owner(const WorldState& world, Side side) {
  return side == Side::kOwn ? world.upstream.our_outpost_owner
                            : world.upstream.enemy_outpost_owner;
}

double clamp01(double value) {
  return std::max(0.0, std::min(1.0, value));
}

}  // namespace

UtilityTaskPolicy::UtilityTaskPolicy(const PolicyConfig& config) : config_(config) {
  if (const auto value = config_.number("utility.safe_hp_ratio")) {
    safe_hp_ratio_ = clamp01(*value);
  }
  if (const auto value = config_.number("utility.low_hp_ratio")) {
    low_hp_ratio_ = clamp01(*value);
  }
  if (const auto value = config_.number("utility.attack_radius")) {
    attack_radius_ = std::max(0.1, *value);
  }
  if (const auto value = config_.number("utility.travel_weight")) {
    travel_weight_ = std::max(0.0, *value);
  }
  if (const auto value = config_.number("utility.switch_penalty")) {
    switch_penalty_ = std::max(0.0, *value);
  }
  if (const auto value = config_.number("utility.continue_bonus")) {
    continue_bonus_ = std::max(0.0, *value);
  }
  if (const auto value = config_.number("utility.last_call_bonus")) {
    last_call_bonus_ = std::max(0.0, *value);
  }
}

bool UtilityTaskPolicy::valid_point(const std::string& name) const {
  return config_.find_point(name) != nullptr;
}

double UtilityTaskPolicy::distance_to(const WorldState& world, const std::string& name) const {
  const Point2D* point = config_.find_point(name);
  if (point == nullptr || !world.self.valid) {
    return 1.0e6;
  }
  return std::hypot(world.self.pose.x - point->x, world.self.pose.y - point->y);
}

double UtilityTaskPolicy::travel_cost(const WorldState& world, const std::string& name) const {
  return distance_to(world, name) * travel_weight_;
}

bool UtilityTaskPolicy::at_point(const WorldState& world, const std::string& name,
                                 double radius) const {
  return distance_to(world, name) <= radius;
}

bool UtilityTaskPolicy::near_any_strike(const WorldState& world, Side side) const {
  const std::string prefix = side == Side::kOwn ? "own_outpost_" : "enemy_outpost_";
  return at_point(world, prefix + "front", attack_radius_) ||
         at_point(world, prefix + "side", attack_radius_) ||
         at_point(world, prefix + "opposite", attack_radius_);
}

bool UtilityTaskPolicy::is_highland(const std::string& name) const {
  return name == point_name(points2027::kOwnOutpostSide) ||
         name == point_name(points2027::kEnemyOutpostSide) ||
         name == point_name(points2027::kOwnOutpostOpposite) ||
         name == point_name(points2027::kEnemyOutpostOpposite);
}

bool UtilityTaskPolicy::is_buff(const std::string& name) const {
  return name == point_name(points2027::kOwnOutpostBuff) ||
         name == point_name(points2027::kEnemyOutpostBuff);
}

bool UtilityTaskPolicy::is_gain_point(const std::string& name) const {
  return is_buff(name) || is_highland(name) ||
         name == point_name(points2027::kOwnBaseSupply);
}

bool UtilityTaskPolicy::free_supply_ammo_pending(const WorldState& world) const {
  const auto duration = config_.number("match.duration_s");
  if (!duration.has_value()) {
    return false;
  }
  const int elapsed =
      std::max(0, static_cast<int>(duration.value()) - world.upstream.game_time_remaining);
  return world.supply_claimed_minutes < elapsed / 60;
}

double UtilityTaskPolicy::outpost_hp_ratio(int hp) const {
  return clamp01(static_cast<double>(hp) / kOutpostMaxHp);
}

UtilityTaskPolicy::Context UtilityTaskPolicy::make_context(const WorldState& world) const {
  Context context;
  const auto& up = world.upstream;

  context.own_full = up.our_outpost_control == OutpostControl::kOwnFull;
  context.own_temp = up.our_outpost_control == OutpostControl::kOwnTemporary;
  context.own_lost = up.our_outpost_control == OutpostControl::kNotOwned;
  context.enemy_full = up.enemy_outpost_control == OutpostControl::kOwnFull;
  context.enemy_temp = up.enemy_outpost_control == OutpostControl::kOwnTemporary;
  context.enemy_lost = up.enemy_outpost_control == OutpostControl::kNotOwned;
  context.own_alive = up.our_outpost_hp > 0;
  context.enemy_alive = up.enemy_outpost_hp > 0;
  context.own_under_attack = world.our_outpost_trend.under_attack;
  context.enemy_under_attack = world.enemy_outpost_trend.under_attack;
  context.own_hp_ratio = outpost_hp_ratio(up.our_outpost_hp);
  context.enemy_hp_ratio = outpost_hp_ratio(up.enemy_outpost_hp);

  context.own_risk = 0.0;
  if (!context.own_full) {
    context.own_risk += context.own_lost ? 1.0 : 0.75;
  }
  if (context.own_under_attack) {
    context.own_risk += 0.45;
  }
  if (context.own_alive && context.own_hp_ratio < low_hp_ratio_) {
    context.own_risk += (low_hp_ratio_ - context.own_hp_ratio) * 1.5;
  }
  if (!context.own_alive) {
    context.own_risk += 0.25;
  }
  context.own_risk = std::min(1.8, context.own_risk);

  context.own_hold = 0.0;
  if (up.our_outpost_owner == OutpostOwner::kOwn &&
      (context.own_full || context.own_temp)) {
    context.own_hold = context.own_full ? 1.0 : 0.7;
  }
  context.enemy_hold = 0.0;
  if (up.enemy_outpost_owner == OutpostOwner::kOwn &&
      (context.enemy_full || context.enemy_temp)) {
    context.enemy_hold = context.enemy_full ? 1.15 : 0.8;
  }

  context.own_attack = 0.0;
  if (up.our_outpost_owner == OutpostOwner::kEnemy && context.own_alive) {
    context.own_attack = 1.0;
    if (context.own_temp) {
      context.own_attack += 0.5;
    } else if (context.own_lost) {
      context.own_attack += 0.35;
    }
    context.own_attack += (1.0 - context.own_hp_ratio) * 0.5;
  }
  // 敌方前哨站已被击毁但归属仍是敌方：需要去增益点把控制拿回来/守住。
  context.enemy_convert = 0.0;
  if (up.enemy_outpost_owner == OutpostOwner::kEnemy && !context.enemy_alive &&
      up.enemy_outpost_control != OutpostControl::kOwnFull) {
    context.enemy_convert = up.enemy_outpost_control == OutpostControl::kOwnTemporary ? 1.2 : 1.0;
  }

  context.enemy_attack = 0.0;
  if (up.enemy_outpost_owner == OutpostOwner::kEnemy && context.enemy_alive) {
    context.enemy_attack = 1.0;
    if (context.enemy_temp) {
      context.enemy_attack += 0.5;
    } else if (context.enemy_lost) {
      context.enemy_attack += 0.35;
    }
    context.enemy_attack += (1.0 - context.enemy_hp_ratio) * 0.5;
  }
  return context;
}

double UtilityTaskPolicy::score_navigate(const WorldState& world, const Context& context,
                                         const std::string& point,
                                         const TaskCall& /*call*/) const {
  const auto& up = world.upstream;
  const double hp_need =
      clamp01(1.0 - static_cast<double>(up.self_hp) / std::max(1, 400));
  double value = 0.0;

  if (point == point_name(points2027::kOwnOutpostBuff)) {
    if (context.own_full && context.own_alive) {
      value = 1.4 * context.own_hold + 1.0 * context.own_risk + 0.5 * hp_need + 0.4;
    } else {
      // buff 失效时，去高地比去 buff 更有意义。
      value = 0.2 + 0.5 * context.own_risk;
    }
  } else if (point == point_name(points2027::kEnemyOutpostBuff)) {
    if (context.enemy_full && context.enemy_alive) {
      value = 1.8 * context.enemy_hold + 0.5 * context.enemy_attack + 0.4 * hp_need;
    } else {
      value = 0.2 + 0.6 * context.enemy_attack;
    }
  } else if (point == point_name(points2027::kOwnOutpostSide) ||
             point == point_name(points2027::kOwnOutpostOpposite)) {
    // 己方完全控制且存活时，buff 是默认防守锚点；高地只在需要争夺/恢复控制时优先。
    value = context.own_full && context.own_alive
                ? 1.0 * context.own_risk + 0.3
                : 3.4 * context.own_risk + 1.4 * context.own_attack + 0.5;
  } else if (point == point_name(points2027::kOwnOutpostFront)) {
    value = context.own_attack > 0.0 ? 0.8 * context.own_attack : 0.15;
  } else if (point == point_name(points2027::kEnemyOutpostSide) ||
             point == point_name(points2027::kEnemyOutpostOpposite)) {
    if (context.enemy_attack > 0.0) {
      value = 1.8 * context.enemy_attack + 0.6 * context.enemy_hold + 0.3;
    } else if (context.enemy_convert > 0.0) {
      value = 2.1 * context.enemy_convert + 0.3;
    } else {
      value = 1.4 * context.enemy_hold + 0.25;
    }
  } else if (point == point_name(points2027::kEnemyOutpostFront)) {
    value = context.enemy_attack > 0.0 ? 0.75 * context.enemy_attack : 0.15;
  } else {
    return -1.0e6;
  }

  // 己方半场有风险时，不要过度深入敌方半场。
  const bool enemy_half = point.rfind("enemy_", 0) == 0;
  if (enemy_half) {
    value -= 0.7 * context.own_risk;
    if (up.self_hp < 100 || up.self_ammo < 40) {
      value -= 0.35;
    }
  }
  value -= travel_cost(world, point);
  return value;
}

double UtilityTaskPolicy::score_heal(const WorldState& world, const Context& context,
                                     const std::string& point, const TaskCall& /*call*/) const {
  const auto& up = world.upstream;
  const int max_hp = static_cast<int>(config_.number("self.max_hp").value_or(400.0));
  const double hp_need = clamp01(1.0 - static_cast<double>(up.self_hp) / std::max(1, max_hp));
  const double low_ammo = config_.number("nav.low_ammo").value_or(50.0);
  const double ammo_need =
      clamp01(1.0 - static_cast<double>(up.self_ammo) / std::max(1.0, low_ammo * 2.0));

  if (point == point_name(points2027::kOwnBaseSupply)) {
    return 2.4 * hp_need + 0.9 * ammo_need + 0.2;
  }
  if (point == point_name(points2027::kOwnOutpostBuff)) {
    return 2.0 * hp_need + 1.2 * context.own_hold + 0.4 * context.own_risk;
  }
  if (point == point_name(points2027::kEnemyOutpostBuff)) {
    return 1.6 * hp_need + 1.2 * context.enemy_hold + 0.2 * context.enemy_attack -
           0.8 * context.own_risk;
  }
  return -1.0e6;
}

double UtilityTaskPolicy::score_outpost(const WorldState& world, const Context& context,
                                        Side side, const TaskCall& /*call*/) const {
  const auto& up = world.upstream;
  const int hp = outpost_hp(world, side);
  const auto control = outpost_control(world, side);
  const auto owner = outpost_owner(world, side);
  const double hp_ratio = outpost_hp_ratio(hp);
  if (owner != OutpostOwner::kEnemy || hp <= 0) {
    return -1.0e6;
  }
  if (!near_any_strike(world, side)) {
    return -1.0e6;
  }

  const double attack = side == Side::kOwn ? context.own_attack : context.enemy_attack;
  double value = 2.5 * attack + 0.8 * (1.0 - hp_ratio);
  if (side == Side::kEnemy) {
    value += 0.35;  // 敌方基地暴露
    // 己方半场有风险时，优先回防，不为了敌方前哨继续深入。
    value -= 1.8 * context.own_risk;
  } else {
    value += 0.4 * context.own_risk;
  }

  // 高地能同时占点；front 只适合收尾。
  const std::string prefix = side == Side::kOwn ? "own_outpost_" : "enemy_outpost_";
  const bool need_capture = control != OutpostControl::kOwnFull;
  const bool at_highland = at_point(world, prefix + "side", attack_radius_) ||
                           at_point(world, prefix + "opposite", attack_radius_);
  const bool at_front = at_point(world, prefix + "front", attack_radius_);
  if (at_highland) {
    value += need_capture ? 0.55 : 0.25;
  }
  if (at_front) {
    // front 只适合收尾；满血/高血量且需要占点时，优先转到高地。
    if (need_capture) {
      value -= hp_ratio > 0.35 ? 1.2 : 0.2;
    } else {
      value += 0.2;
    }
  }
  if (up.self_hp < 100 || up.self_ammo < 30) {
    value -= 0.5;
  }
  return value;
}

double UtilityTaskPolicy::score_scan(const WorldState& world, const Context& context,
                                     const TaskCall& /*call*/) const {
  const auto& up = world.upstream;
  const int max_hp = static_cast<int>(config_.number("self.max_hp").value_or(400.0));
  const double hp_need = clamp01(1.0 - static_cast<double>(up.self_hp) / std::max(1, max_hp));

  if (at_point(world, point_name(points2027::kOwnOutpostBuff), 0.5) && context.own_full &&
      context.own_alive) {
    return 2.0 * context.own_hold + 0.8 * context.own_risk + 0.4 * hp_need + 0.5;
  }
  if (at_point(world, point_name(points2027::kEnemyOutpostBuff), 0.5) && context.enemy_full &&
      context.enemy_alive) {
    return 1.9 * context.enemy_hold + 0.4 * hp_need + 0.4 - 0.6 * context.own_risk;
  }
  if (at_point(world, point_name(points2027::kOwnOutpostSide), 0.5) ||
      at_point(world, point_name(points2027::kOwnOutpostOpposite), 0.5)) {
    return 2.0 * context.own_risk + 0.8 * context.own_attack + 0.35;
  }
  if (at_point(world, point_name(points2027::kEnemyOutpostSide), 0.5) ||
      at_point(world, point_name(points2027::kEnemyOutpostOpposite), 0.5)) {
    return 1.2 * context.enemy_attack + 1.6 * context.enemy_convert +
           0.5 * context.enemy_hold + 0.3 - 0.7 * context.own_risk;
  }
  return 0.3;
}

void UtilityTaskPolicy::add_navigate_candidates(const WorldState& world, const Context& context,
                                                std::vector<Candidate>* out) const {
  const bool at_own_highland =
      at_point(world, point_name(points2027::kOwnOutpostSide), 0.5) ||
      at_point(world, point_name(points2027::kOwnOutpostOpposite), 0.5);
  const bool at_own_buff_valid =
      at_point(world, point_name(points2027::kOwnOutpostBuff), 0.5) && context.own_full &&
      context.own_alive;
  const bool at_enemy_highland =
      at_point(world, point_name(points2027::kEnemyOutpostSide), 0.5) ||
      at_point(world, point_name(points2027::kEnemyOutpostOpposite), 0.5);
  const bool at_enemy_buff_valid =
      at_point(world, point_name(points2027::kEnemyOutpostBuff), 0.5) && context.enemy_full &&
      context.enemy_alive;
  const bool at_own_gain = at_own_highland || at_own_buff_valid;
  const bool at_enemy_gain = at_enemy_highland || at_enemy_buff_valid;

  const std::vector<std::string> points = {
      point_name(points2027::kOwnOutpostBuff),
      point_name(points2027::kEnemyOutpostBuff),
      point_name(points2027::kOwnOutpostSide),
      point_name(points2027::kOwnOutpostOpposite),
      point_name(points2027::kOwnOutpostFront),
      point_name(points2027::kEnemyOutpostSide),
      point_name(points2027::kEnemyOutpostOpposite),
      point_name(points2027::kEnemyOutpostFront),
  };
  for (const auto& point : points) {
    if (!valid_point(point)) {
      continue;
    }
    // 已在目标点附近时不再生成同点 Navigate，避免到达后反复成功/重启。
    if (distance_to(world, point) <= 0.2) {
      continue;
    }
    // 已站在同一阵地的任一增益点时，不要在高地/前哨 buff 之间来回跳；
    // 用 Scan/Outpost 驻留，只有跨半场转场才重新 Navigate。
    if (at_own_gain && point.rfind("own_outpost_", 0) == 0) {
      continue;
    }
    if (at_enemy_gain && point.rfind("enemy_outpost_", 0) == 0) {
      continue;
    }
    // 已站在同一阵地的任一增益点时，不要在高地/前哨 buff 之间来回跳；
    // 用 Scan/Outpost 驻留，只有跨半场转场才重新 Navigate。
    if (at_own_gain && point.rfind("own_outpost_", 0) == 0) {
      continue;
    }
    if (at_enemy_gain && point.rfind("enemy_outpost_", 0) == 0) {
      continue;
    }
    TaskCall call{"Navigate", NavigateParams{point, 0.2}, kNavigateTimeout};
    const double score = score_navigate(world, context, point, call);
    if (score > -1.0e5) {
      out->push_back(Candidate{call, score, "navigate " + point});
    }
  }
}

void UtilityTaskPolicy::add_heal_candidates(const WorldState& world, const Context& context,
                                            std::vector<Candidate>* out) const {
  const int max_hp = static_cast<int>(config_.number("self.max_hp").value_or(400.0));
  const bool hp_not_full = world.upstream.self_hp < max_hp;
  const std::string base(point_name(points2027::kOwnBaseSupply));
  if (valid_point(base) && (hp_not_full || free_supply_ammo_pending(world))) {
    TaskCall call{"Heal", HealParams{base, true}, Duration{0}};
    out->push_back(Candidate{call, score_heal(world, context, base, call), "heal " + base});
  }

  if (hp_not_full) {
    const std::string own(point_name(points2027::kOwnOutpostBuff));
    if (valid_point(own) && context.own_full && context.own_alive) {
      TaskCall call{"Heal", HealParams{own, false}, Duration{0}};
      out->push_back(Candidate{call, score_heal(world, context, own, call), "heal " + own});
    }

    const std::string enemy(point_name(points2027::kEnemyOutpostBuff));
    if (valid_point(enemy) && context.enemy_full && context.enemy_alive) {
      TaskCall call{"Heal", HealParams{enemy, false}, Duration{0}};
      out->push_back(Candidate{call, score_heal(world, context, enemy, call), "heal " + enemy});
    }
  }
}

void UtilityTaskPolicy::add_outpost_candidates(const WorldState& world, const Context& context,
                                               std::vector<Candidate>* out) const {
  for (Side side : {Side::kOwn, Side::kEnemy}) {
    const int hp = outpost_hp(world, side);
    const auto owner = outpost_owner(world, side);
    if (owner != OutpostOwner::kEnemy || hp <= 0 || !near_any_strike(world, side)) {
      continue;
    }
    TaskCall call{"Outpost", OutpostParams{side}, Duration{0}};
    const double score = score_outpost(world, context, side, call);
    if (score > -1.0e5) {
      out->push_back(Candidate{call, score, side == Side::kOwn ? "outpost own" : "outpost enemy"});
    }
  }
}

void UtilityTaskPolicy::add_scan_candidate(const WorldState& world, const Context& context,
                                           std::vector<Candidate>* out) const {
  TaskCall call{"Scan", ScanParams{}, Duration{0}};
  out->push_back(Candidate{call, score_scan(world, context, call), "scan"});
}

std::vector<UtilityTaskPolicy::Candidate> UtilityTaskPolicy::candidates(
    const WorldState& world, const Context& context) const {
  std::vector<Candidate> out;
  add_heal_candidates(world, context, &out);
  add_navigate_candidates(world, context, &out);
  add_outpost_candidates(world, context, &out);
  add_scan_candidate(world, context, &out);
  return out;
}

TaskPlan UtilityTaskPolicy::decide(const WorldState& world,
                                   const std::optional<TaskSnapshot>& active_task,
                                   const HistoryView& history) const {
  TaskPlan plan;
  if (!world.upstream.valid || !world.self.valid ||
      world.upstream.game_status != GameStatus::kRunning) {
    return plan;
  }
  if (world.upstream.self_hp <= 0) {
    plan.call = TaskCall{"Respawn", RespawnParams{}, Duration{0}};
    plan.rationale = "dead -> respawn";
    return plan;
  }

  const Context context = make_context(world);
  std::vector<Candidate> list = candidates(world, context);
  for (auto& candidate : list) {
    if (active_task.has_value() && active_task->status == TaskStatus::kRunning &&
        active_task->call.has_value() && *active_task->call == candidate.call) {
      candidate.score += continue_bonus_;
    } else if (active_task.has_value() && active_task->status == TaskStatus::kRunning) {
      candidate.score -= switch_penalty_;
    }
    if (history.previous_task.has_value() && history.previous_task->call.has_value() &&
        *history.previous_task->call == candidate.call) {
      candidate.score += last_call_bonus_;
    }
  }

  std::sort(list.begin(), list.end(), [](const Candidate& a, const Candidate& b) {
    return a.score > b.score;
  });
  if (list.empty()) {
    return plan;
  }

  plan.call = list.front().call;
  std::ostringstream rationale;
  rationale << list.front().reason << " score=" << list.front().score;
  const std::size_t top = std::min<std::size_t>(3, list.size());
  for (std::size_t i = 1; i < top; ++i) {
    rationale << "; " << list[i].reason << "=" << list[i].score;
  }
  plan.rationale = rationale.str();
  return plan;
}

}  // namespace sentry_decision
