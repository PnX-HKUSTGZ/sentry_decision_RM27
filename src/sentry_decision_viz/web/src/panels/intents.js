import { taskStateName } from '../format.js';

// 任务状态面板：渲染 list_state 的当前 TaskState 与安全急停。
export function createIntentsPanel(el) {
  return {
    render: function (state) {
      const snapshot = state.listState;
      if (!snapshot) {
        el.textContent = '等待 list_state …';
        return;
      }
      let html = '<table>';
      html += '<tr><th>任务</th><td>' + taskStateName(snapshot.task_state || 0) + '</td></tr>';
      html += '<tr><th>急停</th><td>' + (snapshot.safety_emergency ? '是' : '否') + '</td></tr>';
      html += '</table>';
      el.innerHTML = html;
    },
  };
}