#include "sentry_decision_sim/sim_world.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "sentry_decision_core/referee_protocol.hpp"

namespace sentry_decision_sim {
namespace {

bool parse_stance_name(const std::string& name, int* out) {
  if (name == "unknown") {
    *out = 0;
  } else if (name == "attack") {
    *out = 1;
  } else if (name == "defense" || name == "defence") {
    *out = 2;
  } else if (name == "move") {
    *out = 3;
  } else {
    return false;
  }
  return true;
}

bool set_outpost_control(OutpostControlState* state, int value) {
  switch (value) {
    case 0:  // 己方完全控制
      state->held_by_us = true;
      state->full = true;
      break;
    case 1:  // 己方临时控制
      state->held_by_us = true;
      state->full = false;
      break;
    case 2:  // 己方未控制（由对方控制）
      state->held_by_us = false;
      state->full = true;
      break;
    default:
      return false;
  }
  state->challenge_ms = 0;
  state->holder_absent_ms = 0;
  return true;
}

bool parse_task_state(const std::string& name, int* out) {
  if (name == "idle") {
    *out = 0;
  } else if (name == "navigate") {
    *out = 1;
  } else if (name == "scan") {
    *out = 2;
  } else if (name == "outpost") {
    *out = 3;
  } else if (name == "resource") {
    *out = 4;
  } else if (name == "heal") {
    *out = 5;
  } else if (name == "respawn") {
    *out = 6;
  } else {
    return false;
  }
  return true;
}

// 写 info1 bit 19（协议 0x020D：当前是否可以确认免费复活）。
void set_can_free_resurrect(SimWorld* world, bool enabled) {
  if (enabled) {
    world->sentry_info_1 |= (1u << 19);
  } else {
    world->sentry_info_1 &= ~(1u << 19);
  }
}

// 读条完成且已确认时以 10% 上限血复活并清理状态；返回是否发生复活。
bool try_complete_respawn(SimWorld* world, int max_hp) {
  if (world == nullptr || !world->respawn.pending || !world->respawn.confirmed) {
    return false;
  }
  if (world->respawn.current_progress < world->respawn.total_progress) {
    return false;
  }
  // 规则 5.2.2：完成读条复活时血量恢复至上限血量的 10%，射击热量重置为 0。
  world->self_hp = std::max(1, static_cast<int>(std::lround(max_hp * 0.10)));
  world->current_heat = 0;
  world->respawn.pending = false;
  world->respawn.confirmed = false;
  world->respawn.current_progress = 0;
  set_can_free_resurrect(world, false);
  return true;
}

}  // namespace

bool apply_world_field(SimWorld* world, const std::string& field, const ScenarioValue& value,
                       std::string* error) {
  if (value.type == ScenarioValue::Type::kString) {
    *error = "set_world 不支持字符串取值: " + field;
    return false;
  }
  const double number =
      value.type == ScenarioValue::Type::kBool ? (value.boolean ? 1.0 : 0.0) : value.number;
  const int as_int = static_cast<int>(number);

  if (field == "game_status") {
    world->game_status = as_int;
  } else if (field == "game_time_remaining") {
    world->game_time_remaining = as_int;
  } else if (field == "coins") {
    world->coins = as_int;
  } else if (field == "event_code") {
    world->event_code = static_cast<unsigned int>(as_int);
  } else if (field == "detect_color") {
    world->detect_color = as_int;
  } else if (field == "can_rebuild_outpost") {
    world->can_rebuild_outpost = number != 0.0;
  } else if (field == "manual_point_x") {
    world->manual_point_x = number;
  } else if (field == "manual_point_y") {
    world->manual_point_y = number;
  } else if (field == "manual_key") {
    world->manual_key = as_int;
  } else if (field == "enemy_outpost_hp") {
    world->enemy_outpost_hp = as_int;
  } else if (field == "enemy_base_hp") {
    world->enemy_base_hp = as_int;
  } else if (field == "self_hp") {
    world->self_hp = as_int;
  } else if (field == "self_ammo") {
    world->self_ammo = as_int;
  } else if (field == "cooling_value") {
    world->cooling_value = as_int;
  } else if (field == "heat_limit") {
    world->heat_limit = as_int;
  } else if (field == "current_heat") {
    world->current_heat = as_int;
  } else if (field == "energy_ratio") {
    world->energy_ratio = as_int;
  } else if (field == "speed_monitor_angle") {
    world->speed_monitor_angle = number;
  } else if (field == "sentry_info_1") {
    world->sentry_info_1 = static_cast<unsigned int>(as_int);
  } else if (field == "sentry_info_2") {
    world->sentry_info_2 = as_int;
  } else if (field == "sentry_info_3") {
    world->sentry_info_3 = static_cast<std::uint64_t>(number);
  } else if (field == "stance") {
    // 语义字段：写入 sentry_info_2 的 bit 12-13。
    const int value = as_int & 0x3;
    world->sentry_info_2 = (world->sentry_info_2 & ~(0x3 << 12)) | (value << 12);
  } else if (field == "stance_enhanced") {
    if (number != 0.0) {
      world->sentry_info_2 |= (1 << 15);
    } else {
      world->sentry_info_2 &= ~(1 << 15);
    }
  } else if (field == "disengaged") {
    // 语义字段：写入 sentry_info_2 的 bit 0（脱战状态）。
    if (number != 0.0) {
      world->sentry_info_2 |= (1 << 0);
    } else {
      world->sentry_info_2 &= ~(1 << 0);
    }
  } else if (field == "can_free_resurrect") {
    if (number != 0.0) {
      world->sentry_info_1 |= (1u << 19);
    } else {
      world->sentry_info_1 &= ~(1u << 19);
    }
  } else if (field == "can_instant_resurrect") {
    if (number != 0.0) {
      world->sentry_info_1 |= (1u << 20);
    } else {
      world->sentry_info_1 &= ~(1u << 20);
    }
  } else if (field == "instant_resurrect_cost") {
    const unsigned int cost = static_cast<unsigned int>(as_int) & 0x3FFu;
    world->sentry_info_1 = (world->sentry_info_1 & ~(0x3FFu << 21)) | (cost << 21);
  } else if (field == "base_hp") {
    world->base_hp = as_int;
  } else if (field == "our_outpost_hp") {
    world->our_outpost_hp = as_int;
  } else if (field == "our_outpost_control") {
    if (!set_outpost_control(&world->our_outpost_control, as_int)) {
      *error = "our_outpost_control 只能是 0（完全）/ 1（临时）/ 2（未控制）";
      return false;
    }
  } else if (field == "enemy_outpost_control") {
    if (!set_outpost_control(&world->enemy_outpost_control, as_int)) {
      *error = "enemy_outpost_control 只能是 0（完全）/ 1（临时）/ 2（未控制）";
      return false;
    }
  } else if (field == "enemy_at_our_outpost_buff") {
    world->our_outpost_control.enemy_present = number != 0.0;
  } else if (field == "enemy_at_enemy_outpost_buff") {
    world->enemy_outpost_control.enemy_present = number != 0.0;
  } else {
    *error = "未知 set_world 字段: " + field;
    return false;
  }
  return true;
}

bool check_expect(const DecisionView& view, const std::string& field, const ScenarioValue& expected,
                  double tolerance, std::string* error) {
  const double expected_number = expected.type == ScenarioValue::Type::kBool
                                     ? (expected.boolean ? 1.0 : 0.0)
                                     : expected.number;
  const bool expected_bool =
      expected.type == ScenarioValue::Type::kBool ? expected.boolean : (expected.number != 0.0);

  if (field == "task_state") {
    int want = 0;
    if (expected.type == ScenarioValue::Type::kString) {
      if (!parse_task_state(expected.text, &want)) {
        *error = "未知 TaskState: " + expected.text;
        return false;
      }
    } else {
      want = static_cast<int>(expected_number);
    }
    if (view.task_state != want) {
      *error =
          "期望 task_state=" + std::to_string(want) + "，实际=" + std::to_string(view.task_state);
      return false;
    }
    return true;
  }
  if (field == "stance") {
    int want = 0;
    if (expected.type == ScenarioValue::Type::kString) {
      if (!parse_stance_name(expected.text, &want)) {
        *error = "未知姿态: " + expected.text;
        return false;
      }
    } else {
      want = static_cast<int>(expected_number);
    }
    if (view.stance != want) {
      *error = "期望 stance=" + std::to_string(want) + "，实际=" + std::to_string(view.stance);
      return false;
    }
    return true;
  }
  if (field == "has_nav_goal") {
    if (view.has_nav_goal != expected_bool) {
      *error = std::string("期望 has_nav_goal=") + (expected_bool ? "true" : "false") +
               "，实际=" + (view.has_nav_goal ? "true" : "false");
      return false;
    }
    return true;
  }
  if (field == "nav_goal_x" || field == "nav_goal_y") {
    if (!view.has_nav_goal) {
      *error = "期望 " + field + "，但当前没有导航目标";
      return false;
    }
    const double actual = field == "nav_goal_x" ? view.nav_goal_x : view.nav_goal_y;
    if (std::fabs(actual - expected_number) > tolerance) {
      *error = "期望 " + field + "=" + std::to_string(expected_number) +
               "，实际=" + std::to_string(actual);
      return false;
    }
    return true;
  }
  if (field == "has_cmd_vel") {
    if (view.has_cmd_vel != expected_bool) {
      *error = "has_cmd_vel 不匹配";
      return false;
    }
    return true;
  }
  *error = "未知 expect 字段: " + field;
  return false;
}

ExchangeResult exchange_ammo(SimWorld* world, int value) {
  ExchangeResult result;
  if (world == nullptr) {
    result.detail = "世界为空";
    return result;
  }
  if (value <= 0) {
    result.detail = "兑换数量必须为正";
    return result;
  }
  // 非远程兑换 10 金币/10 发（规则 5.3.1）。
  const int cost = value;
  if (world->coins < cost) {
    result.detail =
        "金币不足（需要 " + std::to_string(cost) + "，当前 " + std::to_string(world->coins) + "）";
    return result;
  }
  world->coins -= cost;
  world->self_ammo += value;
  result.accepted = true;
  result.amount = value;
  result.coin_cost = cost;
  result.detail = "发弹量 +" + std::to_string(value) + "，金币 -" + std::to_string(cost);
  return result;
}

ExchangeResult exchange_hp(SimWorld* world, int value, int max_hp) {
  ExchangeResult result;
  if (world == nullptr) {
    result.detail = "世界为空";
    return result;
  }
  if (value <= 0) {
    result.detail = "兑换数量必须为正";
    return result;
  }
  if (world->self_hp >= max_hp) {
    result.detail = "血量已满";
    return result;
  }
  const int cost = value;
  if (world->coins < cost) {
    result.detail =
        "金币不足（需要 " + std::to_string(cost) + "，当前 " + std::to_string(world->coins) + "）";
    return result;
  }
  const int before = world->self_hp;
  world->self_hp = std::min(max_hp, world->self_hp + value);
  const int healed = world->self_hp - before;
  world->coins -= cost;
  result.accepted = true;
  result.amount = healed;
  result.coin_cost = cost;
  result.detail = "血量 +" + std::to_string(healed) + "，金币 -" + std::to_string(cost);
  return result;
}

int supply_heal(SimWorld* world, int max_hp, double ratio) {
  if (world == nullptr || max_hp <= 0 || ratio <= 0.0 || world->self_hp >= max_hp) {
    return 0;
  }
  const int heal = std::max(1, static_cast<int>(std::ceil(max_hp * ratio)));
  const int before = world->self_hp;
  world->self_hp = std::min(max_hp, world->self_hp + heal);
  return world->self_hp - before;
}

bool supply_heal_ready(int seconds_in_zone, double enter_delay_s) {
  if (enter_delay_s <= 0.0) {
    return true;
  }
  return static_cast<double>(seconds_in_zone) >= enter_delay_s;
}

EffectResult apply_effect(SimWorld* world, const std::string& effect, const SimEffects& effects) {
  EffectResult result;
  if (world == nullptr) {
    result.detail = "世界为空";
    return result;
  }
  auto damage = [](int current, double amount) {
    return std::max(0, current - static_cast<int>(amount));
  };
  if (effect == "self_damage") {
    world->self_hp = damage(world->self_hp, effects.self_damage);
    result.new_value = world->self_hp;
    result.detail = "自身血量 -" + std::to_string(static_cast<int>(effects.self_damage));
  } else if (effect == "self_ammo_consume") {
    world->self_ammo = damage(world->self_ammo, effects.self_ammo_consume);
    result.new_value = world->self_ammo;
    result.detail = "自身发弹量 -" + std::to_string(static_cast<int>(effects.self_ammo_consume));
  } else if (effect == "self_death") {
    world->self_hp = 0;
    result.new_value = 0.0;
    result.detail = "自身战亡（血量归零）";
  } else if (effect == "our_outpost_damage") {
    const int before = world->our_outpost_hp;
    const int after = damage(before, effects.our_outpost_damage);
    if (after == 0 && before > 0) {
      destroy_outpost(&world->our_outpost_control, &world->our_outpost_hp, false);
    } else {
      world->our_outpost_hp = after;
    }
    result.new_value = world->our_outpost_hp;
    result.detail =
        "己方前哨站血量 -" + std::to_string(static_cast<int>(effects.our_outpost_damage));
  } else if (effect == "our_outpost_destroy") {
    destroy_outpost(&world->our_outpost_control, &world->our_outpost_hp, false);
    result.new_value = world->our_outpost_hp;
    result.detail = "己方前哨站被摧毁";
  } else if (effect == "our_base_damage") {
    world->base_hp = damage(world->base_hp, effects.our_base_damage);
    result.new_value = world->base_hp;
    result.detail = "己方基地血量 -" + std::to_string(static_cast<int>(effects.our_base_damage));
  } else if (effect == "enemy_outpost_damage") {
    const int before = world->enemy_outpost_hp;
    const int after = damage(before, effects.enemy_outpost_damage);
    if (after == 0 && before > 0) {
      destroy_outpost(&world->enemy_outpost_control, &world->enemy_outpost_hp, true);
    } else {
      world->enemy_outpost_hp = after;
    }
    result.new_value = world->enemy_outpost_hp;
    result.detail =
        "敌方前哨站血量 -" + std::to_string(static_cast<int>(effects.enemy_outpost_damage));
  } else if (effect == "enemy_outpost_destroy") {
    destroy_outpost(&world->enemy_outpost_control, &world->enemy_outpost_hp, true);
    result.new_value = world->enemy_outpost_hp;
    result.detail = "敌方前哨站被摧毁";
  } else if (effect == "enemy_base_damage") {
    world->enemy_base_hp = damage(world->enemy_base_hp, effects.enemy_base_damage);
    result.new_value = world->enemy_base_hp;
    result.detail = "敌方基地血量 -" + std::to_string(static_cast<int>(effects.enemy_base_damage));
  } else {
    result.detail = "未知仿真效果: " + effect;
    return result;
  }
  result.applied = true;
  return result;
}

int claim_supply_ammo(SimWorld* world, int match_elapsed_seconds, bool in_supply) {
  if (world == nullptr || !in_supply || match_elapsed_seconds < 0) {
    return 0;
  }
  const int available_minutes = match_elapsed_seconds / 60;
  const int claimable = available_minutes - world->supply_ammo_claimed_minutes;
  if (claimable <= 0) {
    return 0;
  }
  // 未领取的份额累积在 (available - claimed) 里，占领时一次性领取，符合规则 5.3.2 示例。
  world->supply_ammo_claimed_minutes = available_minutes;
  const int gain = claimable * 100;
  world->self_ammo += gain;
  return gain;
}

RemoteStepResult step_pending_remote(SimWorld* world, int elapsed_ms, int max_hp) {
  RemoteStepResult result;
  if (world == nullptr || elapsed_ms < 0) {
    return result;
  }
  std::vector<PendingRemoteExchange> still_pending;
  still_pending.reserve(world->pending_remote.size());
  for (auto& item : world->pending_remote) {
    item.remaining_ms -= elapsed_ms;
    if (item.remaining_ms > 0) {
      still_pending.push_back(item);
      continue;
    }
    using K = sentry_decision::DecisionActionKind;
    if (item.kind == K::kRemoteAmmoExchange) {
      const int ammo = 100 * item.value;
      world->self_ammo += ammo;
      result.ammo_delivered += ammo;
    } else if (item.kind == K::kRemoteHpExchange) {
      if (world->self_hp <= 0) {
        // 规则 5.2.1：确认后 6 秒内战亡，远程兑换血量无效且金币不返还。
        result.voided += item.value;
      } else {
        const int heal = static_cast<int>(std::ceil(max_hp * 0.6)) * item.value;
        const int before = world->self_hp;
        world->self_hp = std::min(max_hp, world->self_hp + heal);
        result.hp_delivered += world->self_hp - before;
      }
    }
  }
  world->pending_remote.swap(still_pending);
  return result;
}

int respawn_bar_total(int game_time_remaining, int instant_resurrect_count) {
  // 规则 5.2.2：所需复活读条 = 10 + (420 - 战亡时剩余秒数)/10 + 20*累计兑换立即复活次数，
  // 小数部分四舍五入。剩余时长超过 420s（赛前）时按 0 处理。
  const double tens = std::max(0.0, (420.0 - game_time_remaining) / 10.0);
  const int instant_count = std::max(0, instant_resurrect_count);
  return 10 + static_cast<int>(std::lround(tens)) + 20 * instant_count;
}

void sync_respawn(SimWorld* world) {
  if (world == nullptr) {
    return;
  }
  if (world->self_hp > 0) {
    if (world->respawn.pending) {
      world->respawn.pending = false;
      world->respawn.confirmed = false;
      world->respawn.current_progress = 0;
    }
    set_can_free_resurrect(world, false);
    return;
  }
  if (!world->respawn.pending) {
    // 战亡：立即开始读条，总长按战亡瞬间的比赛剩余时长锁定；射击热量重置为 0（规则 5.2.2）。
    world->respawn.pending = true;
    world->respawn.confirmed = false;
    world->respawn.current_progress = 0;
    world->respawn.total_progress =
        respawn_bar_total(world->game_time_remaining, world->respawn.instant_count);
    world->current_heat = 0;
  }
  set_can_free_resurrect(world, true);
}

RespawnStepResult step_respawn(SimWorld* world, int max_hp, bool speed_up) {
  RespawnStepResult result;
  if (world == nullptr || !world->respawn.pending) {
    return result;
  }
  // 规则 5.2.2：每秒 +1；位于补给区增益点或己方基地血量 <4000 时每秒 +4。
  const int step = speed_up ? 4 : 1;
  world->respawn.current_progress =
      std::min(world->respawn.total_progress, world->respawn.current_progress + step);
  result.advanced = true;
  result.progress = world->respawn.current_progress;
  result.total = world->respawn.total_progress;
  const int before = world->self_hp;
  result.revived = try_complete_respawn(world, max_hp);
  result.healed = world->self_hp - before;
  return result;
}

Occupancy evaluate_occupancy(const GainZones& zones, double x, double y) {
  Occupancy occupancy;
  occupancy.supply = zones.supply.contains(x, y);
  occupancy.base_buff = zones.base_buff.contains(x, y);
  occupancy.our_outpost_buff = zones.our_outpost_buff.contains(x, y);
  occupancy.enemy_outpost_buff = zones.enemy_outpost_buff.contains(x, y);
  occupancy.fort_buff = zones.fort_buff.contains(x, y);
  return occupancy;
}

namespace {

// 写 event_code 的 [shift, shift+width) 位，保留其它位。
void set_bits(unsigned int* value, unsigned shift, unsigned width, unsigned bits_value) {
  const unsigned mask = ((1u << width) - 1u) << shift;
  *value = (*value & ~mask) | ((bits_value << shift) & mask);
}

ActionOutcome reject(std::uint8_t code, const std::string& detail) {
  ActionOutcome outcome;
  outcome.accepted = false;
  outcome.code = code;
  outcome.detail = detail;
  return outcome;
}

}  // namespace

void step_outpost_control(OutpostControlState* state, int* hp, bool our_active, int dt_ms) {
  if (state == nullptr || dt_ms <= 0) {
    return;
  }
  const bool holder_active = state->held_by_us ? our_active : state->enemy_present;
  const bool challenger_active = state->held_by_us ? state->enemy_present : our_active;

  if (state->full) {
    if (challenger_active && !holder_active) {
      state->challenge_ms += dt_ms;
      if (state->challenge_ms >= 5000) {
        // 对方完全控制 -> 挑战方临时控制。
        state->held_by_us = !state->held_by_us;
        state->full = false;
        state->challenge_ms = 0;
        state->holder_absent_ms = 0;
      }
    } else if (challenger_active && holder_active) {
      // 双方同时激活：占领进度暂停。
    } else {
      // 连续激活未满 5 s 后离开：进度均匀倒退。
      state->challenge_ms = std::max(0, state->challenge_ms - dt_ms);
    }
    return;
  }

  // 临时控制：控制方连续 5 s 不激活，挑战方转为完全控制。
  if (holder_active) {
    state->holder_absent_ms = 0;
    return;
  }
  state->holder_absent_ms += dt_ms;
  if (state->holder_absent_ms >= 5000) {
    state->held_by_us = !state->held_by_us;
    state->full = true;
    state->challenge_ms = 0;
    state->holder_absent_ms = 0;
    if (hp != nullptr && *hp <= 0) {
      *hp = 500;  // 规则 5.6.1：击毁后完成控制转换，前哨站以 500 血重建。
    }
  }
}

void destroy_outpost(OutpostControlState* state, int* hp, bool by_us) {
  if (state == nullptr || hp == nullptr || *hp <= 0) {
    return;
  }
  const bool destroyer_is_holder = state->held_by_us == by_us;
  if (destroyer_is_holder) {
    if (state->full) {
      // 控制方自己击毁（仿真里只可能来自手动效果）：完全 -> 临时。
      state->full = false;
      *hp = 0;
    } else {
      // 临时控制方击毁：临时 -> 完全，前哨站重建为 500 血。
      state->full = true;
      *hp = 500;
    }
  } else if (state->full) {
    // 对方完全控制的前哨站被击毁：完全 -> 临时（对方），等待其离开 5 s。
    state->full = false;
    *hp = 0;
  } else {
    // 对方临时控制且被非控制方击毁：保持临时，血量归零。
    *hp = 0;
  }
  state->challenge_ms = 0;
  state->holder_absent_ms = 0;
}

void apply_outpost_control_bits(SimWorld* world) {
  if (world == nullptr) {
    return;
  }
  const auto encode = [](const OutpostControlState& state) -> unsigned {
    if (!state.held_by_us) {
      return 2;  // 己方未控制
    }
    return state.full ? 0u : 1u;
  };
  unsigned int event = world->event_code;
  set_bits(&event, 28, 2, encode(world->our_outpost_control));
  set_bits(&event, 30, 2, encode(world->enemy_outpost_control));
  world->event_code = event;
}

void apply_occupancy(SimWorld* world, const Occupancy& occupancy) {
  if (world == nullptr) {
    return;
  }
  unsigned int event = world->event_code;
  // 仿真默认己方为红方：R 堡垒 / 基地 / 前哨增益点分别写 bit 5-7 / 11-13 / 22-24。
  set_bits(&event, 0, 1, occupancy.supply ? 1u : 0u);  // 己方补给区激活
  set_bits(&event, 5, 3, occupancy.fort_buff ? 1u : 0u);
  set_bits(&event, 11, 3, occupancy.base_buff ? 1u : 0u);
  // 协议：仅在己方控制对应阵地时发送增益点状态（完全 / 临时控制都算控制），
  // 否则发送 4。能否兑换 / 回血仍由 EventCode 的完全控制判断。
  const bool our_outpost_controlled = world->our_outpost_control.held_by_us;
  const bool enemy_outpost_controlled = world->enemy_outpost_control.held_by_us;
  set_bits(&event, 22, 3, our_outpost_controlled ? (occupancy.our_outpost_buff ? 1u : 0u) : 4u);
  set_bits(&event, 25, 3, enemy_outpost_controlled ? (occupancy.enemy_outpost_buff ? 1u : 0u) : 4u);
  world->event_code = event;
}

ActionOutcome execute_action(SimWorld* world, const sentry_decision::DecisionAction& action,
                             int max_hp) {
  using K = sentry_decision::DecisionActionKind;
  ActionOutcome outcome;
  if (world == nullptr) {
    return reject(1, "世界为空");
  }
  // 先把复活状态与死活同步（含 info1.bit19），保证下面的前置校验读到最新状态。
  sync_respawn(world);
  const sentry_decision::EventCode event = sentry_decision::decode_event_code(
      static_cast<std::uint32_t>(world->event_code), 0, sentry_decision::TeamColor::kRed);
  const sentry_decision::SentryInfo1 info1 =
      sentry_decision::decode_sentry_info1(static_cast<std::uint32_t>(world->sentry_info_1));
  const sentry_decision::SentryInfo2 info2 =
      sentry_decision::decode_sentry_info2(static_cast<std::uint16_t>(world->sentry_info_2));

  switch (action.kind) {
    case K::kAmmoExchange: {
      // 协议 0x0120 bit 2-12：本场累计兑换发弹量，必须单调递增。
      const int current = info1.ammo_exchanged_local;
      if (action.value < current) {
        return reject(4, "兑换总量必须单调递增");
      }
      const int delta = action.value - current;
      if (delta == 0) {
        outcome.accepted = true;
        outcome.detail = "已是目标值（幂等）";
        return outcome;
      }
      if (!event.local_ammo_exchange_point()) {
        return reject(2, "未占领可兑换增益点（补给区/基地/前哨站）");
      }
      const ExchangeResult result = exchange_ammo(world, delta);
      outcome.accepted = result.accepted;
      outcome.code = result.accepted ? 0 : 3;
      outcome.detail = result.detail;
      if (result.accepted) {
        set_bits(&world->sentry_info_1, 0, 11, action.value);
      }
      return outcome;
    }
    case K::kFreeResurrect: {
      if (action.value < 0 || action.value > 1) {
        return reject(1, "复活电平只能是 0 或 1");
      }
      if (action.value == 0) {
        world->respawn.confirmed = false;
        outcome.accepted = true;
        outcome.detail = "清除确认复活";
        return outcome;
      }
      if (world->self_hp > 0 || !world->respawn.pending) {
        return reject(2, "当前未处于待复活状态");
      }
      // 协议 0x0120 bit 0「确认复活」是电平位：可重复确认；读条完成即复活。
      world->respawn.confirmed = true;
      outcome.accepted = true;
      if (try_complete_respawn(world, max_hp)) {
        outcome.detail = "确认复活：读条完成，血量恢复至 " + std::to_string(world->self_hp);
      } else {
        outcome.detail = "确认复活（读条 " + std::to_string(world->respawn.current_progress) + "/" +
                         std::to_string(world->respawn.total_progress) + "）";
      }
      return outcome;
    }
    case K::kInstantResurrect: {
      if (action.value < 0 || action.value > 1) {
        return reject(1, "复活电平只能是 0 或 1");
      }
      if (action.value == 0) {
        outcome.accepted = true;
        outcome.detail = "清除立即复活";
        return outcome;
      }
      if (world->self_hp > 0 || !world->respawn.pending) {
        return reject(2, "当前未处于待复活状态");
      }
      if (!info1.can_instant_resurrect) {
        return reject(2, "当前不可兑换立即复活");
      }
      const int cost = static_cast<int>(info1.instant_resurrect_cost);
      if (world->coins < cost) {
        return reject(3, "金币不足（立即复活需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      world->self_hp = max_hp;
      world->current_heat = 0;
      // 规则 5.2.2：累计兑换立即复活次数计入之后免费复活的读条长度。
      ++world->respawn.instant_count;
      world->respawn.pending = false;
      world->respawn.confirmed = false;
      world->respawn.current_progress = 0;
      set_can_free_resurrect(world, false);
      outcome.accepted = true;
      outcome.detail = "立即复活：血量回满，金币 -" + std::to_string(cost);
      return outcome;
    }
    case K::kRemoteAmmoExchange: {
      // 协议 0x0120 bit 13-16：本场累计远程兑换次数，单调递增且每次仅能 +1。
      const int current = info1.remote_ammo_exchange_count;
      if (action.value < current) {
        return reject(4, "远程兑换次数必须单调递增");
      }
      const int delta = action.value - current;
      if (delta == 0) {
        outcome.accepted = true;
        outcome.detail = "已是目标值（幂等）";
        return outcome;
      }
      if (delta > 1) {
        return reject(4, "远程兑换次数每次只能 +1");
      }
      if (!info2.disengaged) {
        return reject(2, "未脱战，不能远程兑换发弹量");
      }
      const int cost = 150 * delta;
      if (world->coins < cost) {
        return reject(3, "金币不足（远程兑换需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      // 规则 5.3.2：远程兑换成功后 6 秒生效，先入延迟队列。
      world->pending_remote.push_back(PendingRemoteExchange{K::kRemoteAmmoExchange, delta, 6000});
      set_bits(&world->sentry_info_1, 11, 4, action.value);
      outcome.accepted = true;
      outcome.detail = "远程兑换发弹量 +" + std::to_string(100 * delta) +
                       " 将于 6s 后生效，金币 -" + std::to_string(cost);
      return outcome;
    }
    case K::kRemoteHpExchange: {
      // 协议 0x0120 bit 17-20：本场累计远程兑换次数，单调递增且每次仅能 +1。
      const int current = info1.remote_health_exchange_count;
      if (action.value < current) {
        return reject(4, "远程兑换次数必须单调递增");
      }
      const int delta = action.value - current;
      if (delta == 0) {
        outcome.accepted = true;
        outcome.detail = "已是目标值（幂等）";
        return outcome;
      }
      if (delta > 1) {
        return reject(4, "远程兑换次数每次只能 +1");
      }
      if (!info2.disengaged) {
        return reject(2, "未脱战，不能远程兑换血量");
      }
      // 规则表 5-6：血量远程兑换 50 + ROUNDUP((420-剩余)/60 * 20) 金币/次。
      const int remaining = std::max(0, world->game_time_remaining);
      const int per_cost = static_cast<int>(std::ceil(50.0 + (420 - remaining) / 60.0 * 20.0));
      const int cost = per_cost * delta;
      if (world->coins < cost) {
        return reject(3, "金币不足（远程兑换血量需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      // 规则 5.2.1：确定远程兑换血量 6 秒后 +60% 上限血量；期间战亡则作废。
      world->pending_remote.push_back(PendingRemoteExchange{K::kRemoteHpExchange, delta, 6000});
      set_bits(&world->sentry_info_1, 17, 4, action.value);
      outcome.accepted = true;
      outcome.detail = "远程兑换血量将于 6s 后生效，金币 -" + std::to_string(cost);
      return outcome;
    }
    case K::kNone:
    default:
      return reject(1, "未知动作类型");
  }
}

}  // namespace sentry_decision_sim
