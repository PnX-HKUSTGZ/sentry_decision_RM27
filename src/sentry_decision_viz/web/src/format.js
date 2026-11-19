// 纯函数：把 ROS 消息展平成面板视图模型。
// 不碰 DOM / ROSLIB，因此可在 node 下直接单测（web/test/format.test.mjs）。

import { fieldExtent } from './field.js';

export const TASK_STATES = ['idle', 'navigate', 'scan', 'outpost', 'resource', 'heal', 'respawn'];
export const TASK_STATUSES = ['idle', 'running', 'success', 'failure', 'timeout', 'aborted'];
export const OUTPOST_CONTROLS = ['我方完全控制', '我方临时控制', '敌方控制'];
export const OUTPOST_OWNERS = ['未知', '我方', '敌方'];
export const STANCES = ['unknown', 'attack', 'defense', 'move'];
// 比赛阶段：0 未开始 / 1 准备 / 2 15s自检 / 3 5s倒计时 / 4 比赛中 / 5 结算。
export const GAME_STAGES = ['未开始', '准备', '15s自检', '5s倒计时', '比赛中', '比赛结算'];

export function taskStateName(state) {
  return TASK_STATES[state] || '#' + state;
}

export function taskStatusName(status) {
  return TASK_STATUSES[status] || '#' + status;
}

export function outpostControlName(control) {
  return OUTPOST_CONTROLS[control] || '未知';
}

export function outpostOwnerName(owner) {
  return OUTPOST_OWNERS[owner] || '未知';
}

export function stanceName(stance) {
  return STANCES[stance] || '#' + stance;
}

export function gameStageName(status) {
  return GAME_STAGES[status] || '#' + status;
}

// 秒 -> "MM:SS"（用于比赛倒计时）。
export function formatDuration(ms) {
  const seconds = Math.max(0, Number(ms) || 0) / 1000;
  return seconds.toFixed(1) + ' s';
}

export function formatMmSs(seconds) {
  const total = Math.max(0, Math.floor(Number(seconds) || 0));
  const mm = String(Math.floor(total / 60)).padStart(2, '0');
  const ss = String(total % 60).padStart(2, '0');
  return mm + ':' + ss;
}

export function worldStateToView(msg) {
  return {
    gameStatus: msg.game_status,
    gameStatusName: gameStageName(msg.game_status),
    gameTime: msg.game_time_remaining,
    coins: msg.coins,
    selfHp: msg.self_hp,
    selfAmmo: msg.self_ammo,
    baseHp: msg.base_hp,
    ourOutpostHp: msg.our_outpost_hp,
    ourOutpostControl: msg.our_outpost_control,
    ourOutpostControlName: outpostControlName(msg.our_outpost_control),
    ourOutpostOwner: msg.our_outpost_owner || 0,
    ourOutpostOwnerName: outpostOwnerName(msg.our_outpost_owner || 0),
    enemyOutpostHp: msg.enemy_outpost_hp,
    enemyOutpostControl: msg.enemy_outpost_control,
    enemyOutpostControlName: outpostControlName(msg.enemy_outpost_control),
    enemyOutpostOwner: msg.enemy_outpost_owner || 0,
    enemyOutpostOwnerName: outpostOwnerName(msg.enemy_outpost_owner || 0),
    enemyBaseHp: msg.enemy_base_hp,
    canRebuild: msg.can_rebuild_outpost,
    upstreamValid: msg.upstream_valid,
    stance: msg.stance || 0,
    stanceName: stanceName(msg.stance || 0),
    posX: msg.pos_x,
    posY: msg.pos_y,
    yaw: msg.yaw,
    selfValid: msg.self_valid,
    hasNavGoal: msg.has_nav_goal,
    navGoalX: msg.nav_goal_x,
    navGoalY: msg.nav_goal_y,
    navReached: msg.nav_reached,
    navFailed: msg.nav_failed,
    enemyValid: msg.enemy_valid,
    hasEnemy: msg.has_enemy_position,
    enemyX: msg.enemy_x,
    enemyY: msg.enemy_y,
    allyCount: msg.ally_count,
  };
}

export function decisionToView(msg) {
  const out = msg.output || {};
  const mode = out.task_state || 0;
  return {
    tick: msg.tick,
    mode: mode,
    modeName: taskStateName(mode),
    stance: out.stance || 0,
    stanceName: stanceName(out.stance || 0),
    hasNavGoal: !!out.has_nav_goal,
    navGoalX: (out.nav_goal && out.nav_goal.x) || 0,
    navGoalY: (out.nav_goal && out.nav_goal.y) || 0,
    hasCmdVel: !!out.has_cmd_vel,
  };
}

// 战场模型：范围取自 RMUC 场地底图（origin/resolution/尺寸见 field.js）。
// listState（list_state JSON）携带命名点；重置 / 轮询后会重新读取，覆盖层随之更新。
export function battlefieldModel(world, decision, listState) {
  return {
    extent: fieldExtent(),
    self: world && world.selfValid ? { x: world.posX, y: world.posY, yaw: world.yaw } : null,
    goal: decision && decision.hasNavGoal ? { x: decision.navGoalX, y: decision.navGoalY } : null,
    enemy: world && world.enemyValid && world.hasEnemy ? { x: world.enemyX, y: world.enemyY } : null,
    mode: decision ? decision.modeName : 'unknown',
    points: listState && Array.isArray(listState.points) ? listState.points : [],
  };
}

export function escapeHtml(text) {
  return String(text === null || text === undefined ? '' : text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

export function parseListState(json) {
  if (!json) {
    return null;
  }
  try {
    return JSON.parse(json);
  } catch (error) {
    return null;
  }
}

export function worldToCanvas(point, extent, width, height) {
  const nx = (point.x - extent.minX) / (extent.maxX - extent.minX);
  const ny = (point.y - extent.minY) / (extent.maxY - extent.minY);
  return { x: nx * width, y: (1 - ny) * height };
}
