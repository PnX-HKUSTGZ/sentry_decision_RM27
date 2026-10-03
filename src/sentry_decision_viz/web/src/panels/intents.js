import { escapeHtml } from '../format.js';

// 资源与动作面板：渲染 list_state 的急停 / 资源请求 / 最近动作 / 动作回执。
export function createIntentsPanel(el) {
  return {
    render: function (state) {
      const snapshot = state.listState;
      if (!snapshot) {
        el.textContent = '等待 list_state …';
        return;
      }
      let html = '<table>';
      html += '<tr><th>急停</th><td>' + (snapshot.safety_emergency ? '是' : '否') + '</td></tr>';

      const resource = snapshot.resource || {};
      html +=
        '<tr><th>资源请求</th><td>本地弹=' +
        (resource.ammo || 0) +
        ' 本地血=' +
        (resource.hp || 0) +
        ' 远程弹=' +
        (resource.remote_ammo || 0) +
        ' 远程血=' +
        (resource.remote_hp || 0) +
        ' 复活=' +
        (resource.instant_revive ? '立即' : resource.revive ? '免费' : '否') +
        '</td></tr>';

      const action = snapshot.last_action;
      html += '<tr><th>最近动作</th><td>';
      html += action
        ? escapeHtml(action.kind) + ' value=' + action.value + ' #' + action.request_id
        : '-';
      html += '</td></tr>';

      const ack = snapshot.last_ack;
      html += '<tr><th>动作回执</th><td>';
      html += ack
        ? '#' + ack.request_id + ' ' + (ack.accepted ? 'accepted' : 'rejected') + ' code=' + ack.code +
          (ack.detail ? ' ' + escapeHtml(ack.detail) : '')
        : '-';
      html += '</td></tr>';

      html += '</table>';
      el.innerHTML = html;
    },
  };
}