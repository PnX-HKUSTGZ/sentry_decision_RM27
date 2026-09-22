import { createStore } from './store.js';
import { createBridge } from './bridge.js';
import {
  battlefieldModel,
  decisionToView,
  parseListState,
  treeStatusToView,
  worldStateToView,
} from './format.js';
import { drawBattlefield } from './battlefield.js';
import { createTreePanel } from './panels/tree.js';
import { createWorldPanel } from './panels/world.js';
import { createIntentsPanel } from './panels/intents.js';
import { createControlsPanel } from './panels/controls.js';

const store = createStore({
  connected: false,
  detail: '',
  tree: null,
  world: null,
  decision: null,
  listState: null,
  log: [],
});

function appendLog(text) {
  store.update({ log: store.get().log.concat([text]).slice(-50) });
}

const urlInput = document.getElementById('ws-url');
const connEl = document.getElementById('conn');
const canvas = document.getElementById('battlefield');

const bridge = createBridge({
  url: urlInput.value,
  onStatus: function (connected, detail) {
    store.update({ connected: connected, detail: detail });
  },
  onTree: function (msg) {
    store.update({ tree: treeStatusToView(msg) });
  },
  onWorld: function (msg) {
    store.update({ world: worldStateToView(msg) });
  },
  onDecision: function (msg) {
    store.update({ decision: decisionToView(msg) });
  },
});

const treePanel = createTreePanel(document.getElementById('tree'), document.getElementById('tree-meta'));
const worldPanel = createWorldPanel(document.getElementById('world'));
const intentsPanel = createIntentsPanel(document.getElementById('intents'));
createControlsPanel(document.getElementById('controls'), bridge, appendLog);

store.subscribe(function (state) {
  treePanel.render(state);
  worldPanel.render(state);
  intentsPanel.render(state);
  drawBattlefield(canvas, battlefieldModel(state.world, state.decision));
  connEl.textContent = state.connected ? '已连接' : '未连接';
  connEl.className = 'conn ' + (state.connected ? 'conn-on' : 'conn-off');
  document.getElementById('log').textContent = state.log.join('\n');
});

document.getElementById('connect').addEventListener('click', function () {
  bridge.connect(urlInput.value);
  appendLog('连接 ' + urlInput.value);
});

// list_state 轮询：刷新模块开关、活跃 Intent 与逐字段胜者。
setInterval(function () {
  if (!store.get().connected) {
    return;
  }
  bridge
    .callDebug('list_state', '')
    .then(function (response) {
      if (response && response.success) {
        store.update({ listState: parseListState(response.state_json) });
      }
    })
    .catch(function () {});
}, 1000);

appendLog('就绪：点击「连接」');
