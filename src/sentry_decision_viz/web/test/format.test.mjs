// 纯逻辑单测：node web/test/format.test.mjs（无依赖、无浏览器）。
import {
  battlefieldModel,
  decisionToView,
  escapeHtml,
  nodeStatusName,
  parseListState,
  stanceName,
  tacticalModeName,
  treeStatusToView,
  worldStateToView,
  worldToCanvas,
} from '../src/format.js';

let failures = 0;

function check(ok, message) {
  if (!ok) {
    console.error('FAIL: ' + message);
    failures += 1;
  }
}

check(nodeStatusName(1) === 'RUNNING', 'nodeStatusName');
check(nodeStatusName(9) === '#9', 'nodeStatusName unknown');
check(tacticalModeName(4) === 'retreat', 'tacticalModeName');
check(tacticalModeName(42) === '#42', 'tacticalModeName unknown');
check(stanceName(0) === 'unknown' && stanceName(2) === 'defense', 'stanceName');

const tree = treeStatusToView({
  tick: 3,
  tick_ms: 0.5,
  nodes: [
    { full_path: 'seq', registration_name: 'Sequence', instance_name: 'seq', status: 1 },
    { full_path: 'hold', registration_name: 'Hold', instance_name: 'hold', status: 1 },
    { full_path: 'ok', registration_name: 'Ok', instance_name: 'ok', status: 0 },
  ],
  active_path: ['hold', 'seq'],
});
check(tree.nodes.length === 3, 'tree nodes');
check(tree.activeCount === 2, 'tree activeCount');
check(tree.nodes[0].active === true && tree.nodes[2].active === false, 'tree active flags');
check(tree.nodes[1].statusName === 'RUNNING', 'tree status name');

const world = worldStateToView({
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
const decision = decisionToView({ output: { tactical_mode: 4, has_nav_goal: true, nav_goal: { x: 3, y: 4 } } });
check(decision.modeName === 'retreat', 'decision mode');
const decisionMapped = decisionToView({ output: { tactical_mode: 2, stance: 1 } });
check(decisionMapped.stanceName === 'attack', 'decision stance');

const model = battlefieldModel(world, decision);
check(model.self.x === 1 && model.self.y === 2, 'battlefield self');
check(model.goal.x === 3 && model.goal.y === 4, 'battlefield goal');
check(model.enemy.x === -1 && model.enemy.y === -2, 'battlefield enemy');

const extent = { minX: -8, maxX: 8, minY: -8, maxY: 8 };
const center = worldToCanvas({ x: 0, y: 0 }, extent, 320, 320);
check(center.x === 160 && center.y === 160, 'worldToCanvas center');
const corner = worldToCanvas({ x: -8, y: 8 }, extent, 320, 320);
check(corner.x === 0 && corner.y === 0, 'worldToCanvas corner');

check(parseListState('{"a":1}').a === 1, 'parseListState');
check(parseListState('not json') === null, 'parseListState invalid');
check(escapeHtml('<a>"x"&') === '&lt;a&gt;&quot;x&quot;&amp;', 'escapeHtml');

if (failures !== 0) {
  console.error(failures + ' check(s) failed');
  process.exit(1);
}
console.log('format.test passed');
