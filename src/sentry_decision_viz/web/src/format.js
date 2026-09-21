// 纯函数：把 ROS 消息展平成面板视图模型。
// 不碰 DOM / ROSLIB，因此可在 node 下直接单测（web/test/format.test.mjs）。

export const NODE_STATUS = ['IDLE', 'RUNNING', 'SUCCESS', 'FAILURE', 'SKIPPED'];
export const TACTICAL_MODES = ['unknown', 'patrol', 'attack', 'defend', 'retreat', 'heal', 'respawn'];

export function nodeStatusName(status) {
  return NODE_STATUS[status] || '#' + status;
}

export function tacticalModeName(mode) {
  return TACTICAL_MODES[mode] || '#' + mode;
}

export function treeStatusToView(msg) {
  const active = new Set(msg.active_path || []);
  const nodes = (msg.nodes || []).map(function (n) {
    return {
      path: n.full_path,
      name: n.instance_name,
      type: n.registration_name,
      status: n.status,
      statusName: nodeStatusName(n.status),
      active: active.has(n.full_path),
    };
  });
  return {
    tick: msg.tick || 0,
    tickMs: msg.tick_ms || 0,
    activeCount: active.size,
    nodes: nodes,
  };
}

export function worldStateToView(msg) {
  return {
    gameStatus: msg.game_status,
    gameTime: msg.game_time_remaining,
    coins: msg.coins,
    selfHp: msg.self_hp,
    selfAmmo: msg.self_ammo,
    baseHp: msg.base_hp,
    ourOutpostHp: msg.our_outpost_hp,
    enemyOutpostHp: msg.enemy_outpost_hp,
    enemyBaseHp: msg.enemy_base_hp,
    canRebuild: msg.can_rebuild_outpost,
    refereeValid: msg.referee_valid,
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
    enemyCount: msg.enemy_count,
    allyCount: msg.ally_count,
  };
}

export function decisionToView(msg) {
  const out = msg.output || {};
  const mode = out.tactical_mode || 0;
  return {
    tick: msg.tick,
    mode: mode,
    modeName: tacticalModeName(mode),
    hasNavGoal: !!out.has_nav_goal,
    navGoalX: (out.nav_goal && out.nav_goal.x) || 0,
    navGoalY: (out.nav_goal && out.nav_goal.y) || 0,
    hasCmdVel: !!out.has_cmd_vel,
    resourceAmmo: out.resource_ammo || 0,
    resourceHp: out.resource_hp || 0,
    resourceRevive: !!out.resource_revive,
    conflictCount: msg.conflict_count || 0,
    warnings: msg.warnings || [],
  };
}

// 战场模型：固定地图范围（RMUL 场地约 ±8 m）。
export function battlefieldModel(world, decision) {
  return {
    extent: { minX: -8, maxX: 8, minY: -8, maxY: 8 },
    self: world && world.selfValid ? { x: world.posX, y: world.posY, yaw: world.yaw } : null,
    goal: decision && decision.hasNavGoal ? { x: decision.navGoalX, y: decision.navGoalY } : null,
    enemy: world && world.enemyValid && world.hasEnemy ? { x: world.enemyX, y: world.enemyY } : null,
    mode: decision ? decision.modeName : 'unknown',
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
