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

bool parse_tactical_mode(const std::string& name, int* out) {
  if (name == "unknown") {
    *out = 0;
  } else if (name == "patrol") {
    *out = 1;
  } else if (name == "attack") {
    *out = 2;
  } else if (name == "defend") {
    *out = 3;
  } else if (name == "retreat") {
    *out = 4;
  } else if (name == "heal") {
    *out = 5;
  } else if (name == "respawn") {
    *out = 6;
  } else if (name == "idle") {
    *out = 7;
  } else {
    return false;
  }
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
  } else if (field == "enemy_coin_left") {
    world->enemy_coin_left = as_int;
  } else if (field == "enemy_coin_accumulated") {
    world->enemy_coin_accumulated = as_int;
  } else if (field == "is_enemy_outpost_sensed") {
    world->is_enemy_outpost_sensed = number != 0.0;
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

  if (field == "tactical_mode") {
    int want = 0;
    if (expected.type == ScenarioValue::Type::kString) {
      if (!parse_tactical_mode(expected.text, &want)) {
        *error = "未知战术模式: " + expected.text;
        return false;
      }
    } else {
      want = static_cast<int>(expected_number);
    }
    if (view.tactical_mode != want) {
      *error = "期望 tactical_mode=" + std::to_string(want) +
               "，实际=" + std::to_string(view.tactical_mode);
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
  if (field == "resource_ammo") {
    if (view.resource_ammo != static_cast<int>(expected_number)) {
      *error = "resource_ammo 不匹配";
      return false;
    }
    return true;
  }
  if (field == "resource_hp") {
    if (view.resource_hp != static_cast<int>(expected_number)) {
      *error = "resource_hp 不匹配";
      return false;
    }
    return true;
  }
  if (field == "resource_revive") {
    if (view.resource_revive != expected_bool) {
      *error = "resource_revive 不匹配";
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

Occupancy evaluate_occupancy(const GainZones& zones, double x, double y) {
  Occupancy occupancy;
  occupancy.supply = zones.supply.contains(x, y);
  occupancy.base_buff = zones.base_buff.contains(x, y);
  occupancy.our_outpost_buff = zones.our_outpost_buff.contains(x, y);
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

void apply_occupancy(SimWorld* world, const Occupancy& occupancy) {
  if (world == nullptr) {
    return;
  }
  unsigned int event = world->event_code;
  set_bits(&event, 0, 1, occupancy.supply ? 1u : 0u);  // 己方补给区已占领
  set_bits(&event, 2, 1, occupancy.supply ? 1u : 0u);  // RMUL 同义位
  set_bits(&event, 25, 2, occupancy.fort_buff ? 1u : 0u);
  set_bits(&event, 27, 2, occupancy.our_outpost_buff ? 1u : 0u);
  set_bits(&event, 29, 1, occupancy.base_buff ? 1u : 0u);
  world->event_code = event;
}

ActionOutcome execute_action(SimWorld* world, const sentry_decision::DecisionAction& action,
                             int max_hp) {
  using K = sentry_decision::DecisionActionKind;
  ActionOutcome outcome;
  if (world == nullptr) {
    return reject(1, "世界为空");
  }
  const sentry_decision::EventCode event =
      sentry_decision::decode_event_code(static_cast<std::uint32_t>(world->event_code));
  const sentry_decision::SentryInfo1 info1 =
      sentry_decision::decode_sentry_info1(static_cast<std::uint32_t>(world->sentry_info_1));
  const sentry_decision::SentryInfo2 info2 =
      sentry_decision::decode_sentry_info2(static_cast<std::uint16_t>(world->sentry_info_2));

  switch (action.kind) {
    case K::kAmmoExchange: {
      if (action.value <= 0) {
        return reject(1, "兑换数量必须为正");
      }
      if (!event.local_ammo_exchange_point()) {
        return reject(2, "未占领可兑换增益点（补给区/基地/前哨站）");
      }
      const ExchangeResult result = exchange_ammo(world, action.value);
      outcome.accepted = result.accepted;
      outcome.code = result.accepted ? 0 : 3;
      outcome.detail = result.detail;
      return outcome;
    }
    case K::kHpExchange: {
      if (action.value <= 0) {
        return reject(1, "兑换数量必须为正");
      }
      if (!info2.disengaged) {
        return reject(2, "未脱战，不能兑换血量");
      }
      const ExchangeResult result = exchange_hp(world, action.value, max_hp);
      outcome.accepted = result.accepted;
      outcome.code = result.accepted ? 0 : 3;
      outcome.detail = result.detail;
      return outcome;
    }
    case K::kFreeResurrect: {
      if (!info1.can_free_resurrect) {
        return reject(2, "当前不可确认免费复活");
      }
      outcome.accepted = true;
      outcome.detail = "确认免费复活";
      return outcome;
    }
    case K::kInstantResurrect: {
      if (!info1.can_instant_resurrect) {
        return reject(2, "当前不可兑换立即复活");
      }
      const int cost = static_cast<int>(info1.instant_resurrect_cost);
      if (world->coins < cost) {
        return reject(3, "金币不足（立即复活需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      world->self_hp = max_hp;
      outcome.accepted = true;
      outcome.detail = "立即复活：血量回满，金币 -" + std::to_string(cost);
      return outcome;
    }
    case K::kRemoteAmmoExchange: {
      if (action.value <= 0) {
        return reject(1, "兑换次数必须为正");
      }
      if (!info2.disengaged) {
        return reject(2, "未脱战，不能远程兑换发弹量");
      }
      const int cost = 150 * action.value;
      if (world->coins < cost) {
        return reject(3, "金币不足（远程兑换需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      // 规则 5.3.2：远程兑换成功后 6 秒生效，先入延迟队列。
      world->pending_remote.push_back(
          PendingRemoteExchange{K::kRemoteAmmoExchange, action.value, 6000});
      outcome.accepted = true;
      outcome.detail = "远程兑换发弹量 +" + std::to_string(100 * action.value) +
                       " 将于 6s 后生效，金币 -" + std::to_string(cost);
      return outcome;
    }
    case K::kRemoteHpExchange: {
      if (action.value <= 0) {
        return reject(1, "兑换次数必须为正");
      }
      if (!info2.disengaged) {
        return reject(2, "未脱战，不能远程兑换血量");
      }
      // 规则表 5-6：血量远程兑换 50 + ROUNDUP((420-剩余)/60 * 20) 金币/次。
      const int remaining = std::max(0, world->game_time_remaining);
      const int per_cost = static_cast<int>(std::ceil(50.0 + (420 - remaining) / 60.0 * 20.0));
      const int cost = per_cost * action.value;
      if (world->coins < cost) {
        return reject(3, "金币不足（远程兑换血量需要 " + std::to_string(cost) + "）");
      }
      world->coins -= cost;
      // 规则 5.2.1：确定远程兑换血量 6 秒后 +60% 上限血量；期间战亡则作废。
      world->pending_remote.push_back(
          PendingRemoteExchange{K::kRemoteHpExchange, action.value, 6000});
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
