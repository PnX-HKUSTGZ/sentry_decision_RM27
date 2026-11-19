import { escapeHtml, formatDuration, taskStateName, taskStatusName } from '../format.js';

function row(label, value, className) {
  return (
    '<tr' +
    (className ? ' class="' + className + '"' : '') +
    '><th>' +
    label +
    '</th><td>' +
    value +
    '</td></tr>'
  );
}

function code(value) {
  return value ? '<code>' + escapeHtml(value) + '</code>' : '-';
}

// 任务状态面板：渲染 list_state 的当前 TaskSnapshot 与上一次 TaskResult。
export function createIntentsPanel(el) {
  return {
    render: function (state) {
      const snapshot = state.listState;
      if (!snapshot) {
        el.textContent = '等待 list_state …';
        return;
      }
      const task = snapshot.task;
      const previous = snapshot.previous;
      let html = '<table>';
      html += row('当前任务', task ? escapeHtml(task.id) + ' / ' + taskStateName(task.state) : 'idle');
      html += row('状态 / 阶段', task ? taskStatusName(task.status) + ' / ' + escapeHtml(task.phase) : '-');
      html += row('已进行', task ? formatDuration(task.elapsed_ms) : '-');
      html += row('说明', task && task.detail ? escapeHtml(task.detail) : '-');
      html += row('参数', task ? code(task.params) : '-');
      html += row('指标', task ? code(task.metrics) : '-');
      html += row(
        '上一次',
        previous ? escapeHtml(previous.id) + ' / ' + taskStatusName(previous.status) : '-',
        'section-start'
      );
      html += row('上次用时', previous ? formatDuration(previous.elapsed_ms) : '-');
      html += row('上次原因', previous && previous.reason ? escapeHtml(previous.reason) : '-');
      html += row('上次指标', previous ? code(previous.metrics) : '-');
      html += row('急停', snapshot.safety_emergency ? '是' : '否');
      html += '</table>';
      el.innerHTML = html;
    },
  };
}
