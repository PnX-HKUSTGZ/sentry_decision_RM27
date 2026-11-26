// 仿真控件：比赛阶段 / 仿真世界 / 仿真效果 -> 裁判仿真 service。
// 面板不使用 ROS action 客户端：vendored roslib 1.4.1 的 ActionClient 是 ROS 1
// actionlib 命名，无法对接 ROS 2 action，故面板统一走 service。
export function createControlsPanel(el, bridge, onLog, onRefresh) {
  const worldFields = [
    ['self_hp', '自身血量'],
    ['self_ammo', '自身弹量'],
    ['coins', '金币'],
    ['base_hp', '己方基地'],
    ['our_outpost_hp', '己方前哨'],
    ['enemy_outpost_hp', '敌方前哨'],
    ['enemy_base_hp', '敌方基地'],
    ['game_time_remaining', '剩余时间'],
    ['our_outpost_control', '己方前哨控制 0/1/2'],
    ['enemy_outpost_control', '敌方前哨控制 0/1/2'],
    ['enemy_at_our_outpost_buff', '敌方占己方前哨 0/1'],
    ['enemy_at_enemy_outpost_buff', '敌方占敌方前哨 0/1'],
  ];
  let html = '';
  html += '<div class="row"><span>比赛</span>';
  html += '<button id="btn-stage-1">准备</button>';
  html += '<button id="btn-stage-2">15s自检</button>';
  html += '<button id="btn-stage-3">5s倒计时</button>';
  html += '<button id="btn-stage-4">开始比赛</button>';
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

  // 前哨快捷操作：直接改控制状态与敌方占点标志，省去逐项选字段。
  const outpostRows = [
    ['己方前哨', [
      ['完全控制', 'our_outpost_control', 0],
      ['临时控制', 'our_outpost_control', 1],
      ['未控制', 'our_outpost_control', 2],
      ['敌方占点', 'enemy_at_our_outpost_buff', 1],
      ['敌方离开', 'enemy_at_our_outpost_buff', 0],
    ]],
    ['对方前哨', [
      ['我方完全', 'enemy_outpost_control', 0],
      ['我方临时', 'enemy_outpost_control', 1],
      ['敌方控制', 'enemy_outpost_control', 2],
      ['敌方占点', 'enemy_at_enemy_outpost_buff', 1],
      ['敌方离开', 'enemy_at_enemy_outpost_buff', 0],
    ]],
  ];
  outpostRows.forEach(function (group) {
    html += '<div class="row"><span>' + group[0] + '</span>';
    group[1].forEach(function (item) {
      html +=
        '<button class="sim-world-quick" data-field="' +
        item[1] +
        '" data-value="' +
        item[2] +
        '">' +
        item[0] +
        '</button>';
    });
    html += '</div>';
  });
  html += '<pre id="log" class="log"></pre>';
  el.innerHTML = html;

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
    // 重置裁判仿真的世界与位姿。
    bridge
      .setGameStage(0)
      .then(function () {
        onLog('比赛已重置到未开始（含位置）');
        // 重新读取 list_state，使面板叠加的命名点等随重置刷新。
        if (onRefresh) {
          onRefresh();
        }
      })
      .catch(function (error) {
        onLog('重置失败: ' + error);
      });
  });

  function setSimWorld(field, value) {
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
  }

  document.getElementById('btn-sim-world-set').addEventListener('click', function () {
    const field = document.getElementById('sim-world-field').value;
    const value = parseFloat(document.getElementById('sim-world-value').value);
    setSimWorld(field, value);
  });

  // 前哨快捷按钮：data-field / data-value -> set_world。
  Array.prototype.forEach.call(document.querySelectorAll('.sim-world-quick'), function (button) {
    button.addEventListener('click', function () {
      setSimWorld(button.getAttribute('data-field'), Number(button.getAttribute('data-value')));
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