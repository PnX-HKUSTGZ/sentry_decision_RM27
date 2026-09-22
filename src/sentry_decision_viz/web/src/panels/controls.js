// 人工干预控件：比赛阶段 -> /sentry_sim/set_game_stage；其余走 /decision/debug (service)。
// 不使用 ManualOverride action：vendored roslib 1.4.1 的 ActionClient 是 ROS 1 actionlib
// 命名（/<action>/goal 等），无法对接 ROS 2 action 的 /_action/* 服务，故面板统一走 service。
export function createControlsPanel(el, bridge, onLog) {
  const options = ['patrol', 'attack', 'defend', 'retreat', 'heal', 'respawn'];
  let html = '';
  html += '<div class="row"><span>比赛阶段</span>';
  html += '<button id="btn-stage-1">准备</button>';
  html += '<button id="btn-stage-2">15s自检</button>';
  html += '<button id="btn-stage-3">5s倒计时</button>';
  html += '<button id="btn-stage-4">开始比赛</button>';
  html += '<button id="btn-reset">重置</button></div>';
  html += '<div class="row"><button id="btn-retreat">强制撤退</button>';
  html += '<button id="btn-clear">清空干预</button></div>';
  html += '<div class="row"><span>模式</span><select id="mode-select">';
  options.forEach(function (name) {
    html += '<option value="' + name + '">' + name + '</option>';
  });
  html += '</select><input id="lease" value="5" size="3" title="lease 秒"/><button id="btn-mode">切换</button></div>';
  html += '<div class="row"><span>点位</span><input id="point-x" value="0" size="4"/>';
  html += '<input id="point-y" value="0" size="4"/><button id="btn-point">前往</button></div>';
  html += '<div class="row"><span>模块</span><select id="module-select">';
  ['nav', 'strategic', 'resource', 'recovery'].forEach(function (name) {
    html += '<option value="' + name + '">' + name + '</option>';
  });
  html += '</select><button id="btn-module-on">启用</button><button id="btn-module-off">禁用</button></div>';
  html += '<div class="row"><button id="btn-ammo">兑换发弹</button><button id="btn-hp">兑换血量</button></div>';
  html += '<pre id="log" class="log"></pre>';
  el.innerHTML = html;

  function lease() {
    const value = parseFloat(document.getElementById('lease').value);
    return value > 0 ? value : 5;
  }

  // 人工意图统一走 /decision/debug set_intent（service）。
  function setIntent(field, value, label) {
    const args =
      '{field: ' + field + ', value: ' + value + ', lease_sec: ' + lease() +
      ", reason: 'web-" + label + "'}";
    debug('set_intent', args, label);
  }

  function debug(command, args, label) {
    bridge
      .callDebug(command, args)
      .then(function (response) {
        onLog(label + ': ' + (response && response.success ? 'ok' : '失败 ' + (response && response.message)));
      })
      .catch(function (error) {
        onLog(label + ' 失败: ' + error);
      });
  }

  // 比赛阶段：0 重置，1-4 快进（服务端负责拒绝回退）。
  function setStage(stage, label) {
    bridge
      .setGameStage(stage)
      .then(function (response) {
        const ok = response && response.success;
        onLog(label + ': ' + (ok ? 'ok' : '失败 ' + (response && response.message)));
      })
      .catch(function (error) {
        onLog(label + ' 失败: ' + error + '（裁判仿真节点未运行？）');
      });
  }

  const stageButtons = [
    { stage: 1, el: document.getElementById('btn-stage-1') },
    { stage: 2, el: document.getElementById('btn-stage-2') },
    { stage: 3, el: document.getElementById('btn-stage-3') },
    { stage: 4, el: document.getElementById('btn-stage-4') },
  ];
  stageButtons.forEach(function (item) {
    item.el.addEventListener('click', function () {
      setStage(item.stage, '比赛阶段 ' + item.stage);
    });
  });

  document.getElementById('btn-reset').addEventListener('click', function () {
    // 重置裁判仿真的世界，并清空决策节点的干预 / 世界覆盖 / 模块开关。
    bridge
      .setGameStage(0)
      .then(function () {
        onLog('比赛已重置到未开始');
        return bridge.callDebug('clear_all', '');
      })
      .then(function () {
        onLog('决策节点干预状态已清空');
      })
      .catch(function (error) {
        onLog('重置失败: ' + error);
      });
  });

  document.getElementById('btn-retreat').addEventListener('click', function () {
    setIntent('tactical_mode', 'retreat', '强制撤退');
  });
  document.getElementById('btn-clear').addEventListener('click', function () {
    debug('clear_all', '', '清空干预');
  });
  document.getElementById('btn-mode').addEventListener('click', function () {
    setIntent('tactical_mode', document.getElementById('mode-select').value, '切换模式');
  });
  document.getElementById('btn-point').addEventListener('click', function () {
    const x = parseFloat(document.getElementById('point-x').value) || 0;
    const y = parseFloat(document.getElementById('point-y').value) || 0;
    setIntent('nav_goal', '[' + x + ', ' + y + ']', '前往点位');
  });
  document.getElementById('btn-module-on').addEventListener('click', function () {
    const name = document.getElementById('module-select').value;
    debug('set_module', '{module: ' + name + ', enabled: true}', '启用 ' + name);
  });
  document.getElementById('btn-module-off').addEventListener('click', function () {
    const name = document.getElementById('module-select').value;
    debug('set_module', '{module: ' + name + ', enabled: false}', '禁用 ' + name);
  });
  document.getElementById('btn-ammo').addEventListener('click', function () {
    setIntent('resource_request', '{ammo: 50, hp: 0, revive: false}', '兑换发弹');
  });
  document.getElementById('btn-hp').addEventListener('click', function () {
    setIntent('resource_request', '{ammo: 0, hp: 50, revive: false}', '兑换血量');
  });

  return {
    render: function (state) {
      const current = state.world ? state.world.gameStatus : 0;
      // 只允许前进：阶段 <= 当前阶段时禁用（0 未开始时全部可用）。
      stageButtons.forEach(function (item) {
        item.el.disabled = item.stage <= current;
      });
    },
  };
}
