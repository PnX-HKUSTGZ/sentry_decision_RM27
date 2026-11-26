// 纯逻辑单测：node web/test/format.test.mjs（无依赖、无浏览器）。
import {
  battlefieldModel,
  decisionToView,
  escapeHtml,
  formatDuration,
  formatMmSs,
  gameStageName,
  parseListState,
  outpostControlName,
  outpostOwnerName,
  stanceName,
  taskStateName,
  taskStatusName,
  worldStateToView,
  worldToCanvas,
} from '../src/format.js';
import { FIELD, fieldExtent, imageToWorld, worldToImage } from '../src/field.js';

let failures = 0;

function check(ok, message) {
  if (!ok) {
    console.error('FAIL: ' + message);
    failures += 1;
  }
}

check(taskStateName(5) === 'heal', 'taskStateName');
check(taskStateName(42) === '#42', 'taskStateName unknown');
check(stanceName(0) === 'unknown' && stanceName(2) === 'defense', 'stanceName');
check(gameStageName(2) === '15s自检', 'gameStageName');
check(gameStageName(99) === '#99', 'gameStageName unknown');
check(formatMmSs(180) === '03:00', 'formatMmSs 3min');
check(formatMmSs(5) === '00:05', 'formatMmSs seconds');
check(formatMmSs(0) === '00:00', 'formatMmSs zero');
check(taskStateName(0) === 'idle', 'taskStateName idle');
check(taskStatusName(2) === 'success' && taskStatusName(99) === '#99', 'taskStatusName');
check(outpostControlName(0) === '我方完全控制', 'outpostControlName full');
check(outpostControlName(2) === '敌方控制', 'outpostControlName enemy');
check(outpostControlName(9) === '未知', 'outpostControlName unknown');
check(outpostOwnerName(0) === '未知' && outpostOwnerName(1) === '我方' && outpostOwnerName(2) === '敌方', 'outpostOwnerName');
check(formatDuration(1500) === '1.5 s', 'formatDuration');

const world = worldStateToView({
  our_outpost_control: 1,
  enemy_outpost_control: 2,
  our_outpost_owner: 2,
  enemy_outpost_owner: 1,
  pos_x: 1,
  pos_y: 2,
  yaw: 0.5,
  self_valid: true,
  has_nav_goal: true,
  nav_goal_x: 3,
  nav_goal_y: 4,
  enemy_valid: true,
  has_enemy_position: true,
  enemy_x: -1,
  enemy_y: -2,
});
check(world.posX === 1 && world.navGoalY === 4, 'world view');
check(world.ourOutpostControlName === '我方临时控制', 'world our outpost control');
check(world.enemyOutpostControlName === '敌方控制', 'world enemy outpost control');
check(world.ourOutpostOwnerName === '敌方' && world.enemyOutpostOwnerName === '我方', 'world outpost owners');
const decision = decisionToView({ output: { task_state: 5, has_nav_goal: true, nav_goal: { x: 3, y: 4 } } });
check(decision.modeName === 'heal', 'decision mode');
const decisionMapped = decisionToView({ output: { task_state: 3, stance: 1 } });
check(decisionMapped.stanceName === 'attack', 'decision stance');

const model = battlefieldModel(world, decision);
check(model.self.x === 1 && model.self.y === 2, 'battlefield self');
check(model.goal.x === 3 && model.goal.y === 4, 'battlefield goal');
check(model.enemy.x === -1 && model.enemy.y === -2, 'battlefield enemy');
const zoned = battlefieldModel(world, decision, {
  points: [{ name: 'sim_home', x: -11.79, y: -3.66, r: 1.5 }],
});
check(zoned.points.length === 1 && zoned.points[0].r === 1.5, 'battlefield zone radius passthrough');

const extent = { minX: -8, maxX: 8, minY: -8, maxY: 8 };
const center = worldToCanvas({ x: 0, y: 0 }, extent, 320, 320);
check(center.x === 160 && center.y === 160, 'worldToCanvas center');
const corner = worldToCanvas({ x: -8, y: 8 }, extent, 320, 320);
check(corner.x === 0 && corner.y === 0, 'worldToCanvas corner');

const field = fieldExtent(FIELD);
check(Math.abs(field.maxX - 14.55) < 1e-9, 'field extent maxX');
check(Math.abs(field.maxY - 9.14) < 1e-9, 'field extent maxY');
const originPoint = { x: FIELD.origin.x, y: FIELD.origin.y };
const originPx = worldToImage(originPoint, FIELD, 2);
check(originPx.x === 0 && originPx.y === FIELD.heightPx * 2, 'field origin maps to bottom-left');
const roundTrip = imageToWorld(originPx.x, originPx.y, FIELD, 2);
check(
  Math.abs(roundTrip.x - originPoint.x) < 1e-9 && Math.abs(roundTrip.y - originPoint.y) < 1e-9,
  'field round trip'
);

check(parseListState('{"a":1}').a === 1, 'parseListState');
check(parseListState('not json') === null, 'parseListState invalid');
check(escapeHtml('<a>"x"&') === '&lt;a&gt;&quot;x&quot;&amp;', 'escapeHtml');

if (failures !== 0) {
  console.error(failures + ' check(s) failed');
  process.exit(1);
}
console.log('format.test passed');
