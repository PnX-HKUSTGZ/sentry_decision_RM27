import { escapeHtml } from '../format.js';

// 模块与干预面板：渲染 list_state 的模块开关、活跃 Intent、胜者与世界覆盖。
export function createIntentsPanel(el) {
  return {
    render: function (state) {
      const snapshot = state.listState;
      if (!snapshot) {
        el.textContent = '等待 list_state …';
        return;
      }
      let html = '<table><tr><th>模块</th><td>';
      const modules = snapshot.modules || {};
      const moduleNames = Object.keys(modules);
      html += moduleNames.length
        ? moduleNames
            .map(function (name) {
              return escapeHtml(name) + '=' + (modules[name] ? 'on' : 'off');
            })
            .join(' ')
        : '-';
      html += '</td></tr><tr><th>急停</th><td>' + (snapshot.safety_emergency ? '是' : '否') + '</td></tr>';

      html += '<tr><th>Intent</th><td>';
      const intents = snapshot.intents || [];
      html += intents.length
        ? intents
            .map(function (intent) {
              return (
                escapeHtml(intent.field) +
                '/' +
                escapeHtml(intent.source) +
                (intent.effective ? ' ✔' : ' ✘')
              );
            })
            .join('<br/>')
        : '-';
      html += '</td></tr>';

      html += '<tr><th>胜者</th><td>';
      const winners = snapshot.winners || {};
      const winnerNames = Object.keys(winners);
      html += winnerNames.length
        ? winnerNames
            .map(function (field) {
              return escapeHtml(field) + '→' + escapeHtml(winners[field]);
            })
            .join('<br/>')
        : '-';
      html += '</td></tr>';

      html += '<tr><th>世界覆盖</th><td>';
      const overrides = snapshot.world_overrides || {};
      const overrideNames = Object.keys(overrides);
      html += overrideNames.length
        ? overrideNames
            .map(function (field) {
              return escapeHtml(field) + '=' + overrides[field];
            })
            .join(' ')
        : '-';
      html += '</td></tr></table>';
      el.innerHTML = html;
    },
  };
}
