// 仿真与战术层覆盖控件：比赛阶段 / 暂停 / 仿真世界 / 仿真效果 -> 裁判仿真 service；
// 战术层覆盖 -> /decision/debug (service)。面板不使用 ROS action 客户端：vendored roslib
// 1.4.1 的 ActionClient 是 ROS 1 actionlib 命名，无法对接 ROS 2 action，故面板统一走 service。
export function createControlsPanel(el, bridge, onLog, onRefresh) {
  const modeOptions = ['patrol', 'attack', 'defend', 'retreat', 'heal', 'respawn'];
  const worldFields = [
    ['self_hp', '自身血量'],
    ['self_ammo', '自身弹量'],
    ['coins', '金币'],
    ['base_hp', '己方基地'],
    ['our_outpost_hp', '己方前哨'],
    ['enemy_outpost_hp', '敌方前哨'],
    ['enemy_base_hp', '敌方基地'],
    ['game_time_remaining', '剩余时间'],
  ];
  let html = '';
  html += '<div class="row"><span>比赛</span>';
  html += '<button id="btn-stage-1">准备</button>';
  html += '<button id="btn-stage-2">15s自检</button>';
  html += '<button id="btn-stage-3">5s倒计时</button>';
  html += '<button id="btn-stage-4">开始比赛</button>';
  html += '<button id="btn-pause">暂停</button>';
  html += '<button id="btn-reset">重置</button></div>';
  // 仿真世界：直接改裁判仿真的真实世界（兑换/回血据此演变）。
  html += '<div class="row"><span title="直接修改裁判仿真的真实世界">仿真世界</span><select id="sim-world-field">';
  worldFields.forEach(function (item) {
    html += '<option value="' + item[0] + '">' + item[1] + '</option>';
  });
  html += '</select><input id="sim-world-value" value="400" size="4"/>';
  html += '<button id="btn-sim-world-set">设置</button></div>';
  // 仿真效果：对真实世界施加具名效果（步长来自 config/sim.yaml 的 effects），
  // 用于模拟赛场事件（自身受击 / 发弹、双方前哨与基地被击毁）。
  const effectRows = [
    ['自身', [
      ['self_damage', '扣血'],
      ['self_ammo_consume', '扣弹'],
      ['self_death', '死亡'],
    ]],
    ['我方', [
      ['our_outpost_damage', '前哨站扣血'],
      ['our_outpost_destroy', '前哨站摧毁'],
      ['our_base_damage', '基地扣血'],
    ]],
    ['敌方', [
      ['enemy_outpost_damage', '前哨站扣血'],
      ['enemy_outpost_destroy', '前哨站摧毁'],
      ['enemy_base_damage', '基地扣血'],
    ]],
  ];
  effectRows.forEach(function (group) {
    html += '<div class="row"><span>' + group[0] + '</span>';
    group[1].forEach(function (item) {
      html +=
        '<button class="sim-effect" data-effect="' + item[0] + '">' + item[1] + '</button>';
    });
    html += '</div>';
  });
  // 战术层覆盖：设置 / 清除期望 TacticalMode（行为树之外，安全层优先级不变）。
  html += '<div class="row"><span>战术模式</span><select id="mode-select">';
  modeOptions.forEach(function (name) {
    html += '<option value="' + name + '">' + name + '</option>';
  });
  html += '</select></div>';
  html += '<div class="row"><span>持续</span><input id="lease" value="0" size="4"/>';
  html += '<span class="meta">秒：0 = 持续到「清除 / 重置」，正数则到期自动交还战略层</span></div>';
  html += '<div class="row"><button id="btn-tactical-set">设置覆盖</button>';
  html += '<button id="btn-tactical-clear">清除</button></div>';
  html += '<pre id="log" class="log"></pre>';
  el.innerHTML = html;

  // lease 秒；0 表示持续到「清除 / 重置」，负数按 0 处理。
  function lease() {
    const value = parseFloat(document.getElementById('lease').value);
    return isFinite(value) && value > 0 ? value : 0;
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

  // 暂停 / 恢复：冻结比赛计时与机器人运动。
  let paused = false;
  const pauseButton = document.getElementById('btn-pause');
  pauseButton.addEventListener('click', function () {
    bridge
      .setGamePause(!paused)
      .then(function (response) {
        paused = !!(response && response.paused);
        pauseButton.textContent = paused ? '继续' : '暂停';
        onLog(response && response.message ? response.message : paused ? '已暂停' : '已恢复');
      })
      .catch(function (error) {
        onLog('暂停失败: ' + error);
      });
  });

  document.getElementById('btn-reset').addEventListener('click', function () {
    // 重置裁判仿真的世界与位姿，并清除决策节点的战术层覆盖。
    bridge
      .setGameStage(0)
      .then(function () {
        paused = false;
        pauseButton.textContent = '暂停';
        onLog('比赛已重置到未开始（含位置）');
        return bridge.callDebug('clear_tactical_mode', '');
      })
      .then(function () {
        onLog('战术层覆盖已清除');
        // 重新读取 list_state，使面板叠加的命名点等随重置刷新。
        if (onRefresh) {
          onRefresh();
        }
      })
      .catch(function (error) {
        onLog('重置失败: ' + error);
      });
  });

  document.getElementById('btn-sim-world-set').addEventListener('click', function () {
    const field = document.getElementById('sim-world-field').value;
    const value = parseFloat(document.getElementById('sim-world-value').value);
    if (!isFinite(value)) {
      onLog('仿真世界: 取值无效');
      return;
    }
    bridge
      .setSimWorld(field, value)
      .then(function (response) {
        const ok = response && response.success;
        onLog('仿真世界 ' + field + ': ' + (ok ? 'ok' : '失败 ' + (response && response.message)));
      })
      .catch(function (error) {
        onLog('仿真世界设置失败: ' + error + '（裁判仿真节点未运行？）');
      });
  });

  // 仿真效果按钮统一走 /sentry_sim/apply_effect；步长在 sim.yaml，前端只传效果名。
  Array.prototype.forEach.call(document.querySelectorAll('.sim-effect'), function (button) {
    button.addEventListener('click', function () {
      const effect = button.getAttribute('data-effect');
      bridge
        .applyEffect(effect)
        .then(function (response) {
          const ok = response && response.success;
          onLog(
            '仿真效果 ' + effect + ': ' +
              (ok ? 'ok (' + response.message + ')' : '失败 ' + (response && response.message))
          );
        })
        .catch(function (error) {
          onLog('仿真效果失败: ' + error + '（裁判仿真节点未运行？）');
        });
    });
  });

  document.getElementById('btn-tactical-set').addEventListener('click', function () {
    const args =
      '{mode: ' + document.getElementById('mode-select').value + ', lease_sec: ' + lease() + '}';
    debug('set_tactical_mode', args, '设置战术层覆盖');
  });
  document.getElementById('btn-tactical-clear').addEventListener('click', function () {
    debug('clear_tactical_mode', '', '清除战术层覆盖');
  });

  // 未进入「比赛中」时禁用战术层覆盖控件（仿真世界 / 仿真效果仍可用）。
  const tacticalElements = ['btn-tactical-set', 'btn-tactical-clear', 'mode-select', 'lease'].map(
    function (id) {
      return document.getElementById(id);
    }
  );

  return {
    render: function (state) {
      const current = state.world ? state.world.gameStatus : 0;
      // 只允许前进：阶段 <= 当前阶段时禁用（0 未开始时全部可用）。
      stageButtons.forEach(function (item) {
        item.el.disabled = item.stage <= current;
      });
      // 只有 game_status=4（比赛中）才接受战术层覆盖。
      const running = current === 4;
      tacticalElements.forEach(function (element) {
        element.disabled = !running;
      });
      if (!running) {
        document.getElementById('mode-select').title = '比赛开始后才能设置战术层覆盖';
      }
    },
  };
}