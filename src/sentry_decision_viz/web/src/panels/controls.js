// 人工干预控件：按钮 -> /decision/debug (service) 与 /decision/manual_override (action)。
export function createControlsPanel(el, bridge, onLog) {
  const options = ['patrol', 'attack', 'defend', 'retreat', 'heal', 'respawn'];
  let html = '';
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

  function override(field, value, label) {
    bridge
      .sendManualOverride(field, value, lease(), 'web-' + label)
      .then(function () {
        onLog(label + ' 已结束');
      })
      .catch(function (error) {
        onLog(label + ' 失败: ' + error);
      });
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

  document.getElementById('btn-retreat').addEventListener('click', function () {
    override(3, 'retreat', '强制撤退');
  });
  document.getElementById('btn-clear').addEventListener('click', function () {
    debug('clear_all', '', '清空干预');
  });
  document.getElementById('btn-mode').addEventListener('click', function () {
    override(3, document.getElementById('mode-select').value, '切换模式');
  });
  document.getElementById('btn-point').addEventListener('click', function () {
    const x = parseFloat(document.getElementById('point-x').value) || 0;
    const y = parseFloat(document.getElementById('point-y').value) || 0;
    override(0, '[' + x + ', ' + y + ']', '前往点位');
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
    override(2, '{ammo: 50, hp: 0, revive: false}', '兑换发弹');
  });
  document.getElementById('btn-hp').addEventListener('click', function () {
    override(2, '{ammo: 0, hp: 50, revive: false}', '兑换血量');
  });

  return {
    render: function () {},
  };
}
