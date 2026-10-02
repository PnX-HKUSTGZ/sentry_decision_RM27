import { escapeHtml } from '../format.js';

// 行为树面板：逐节点显示状态，active path 高亮。
export function createTreePanel(el, metaEl) {
  return {
    render: function (state) {
      const tree = state.tree;
      if (!tree) {
        el.textContent = '等待 /decision/tree_status …';
        return;
      }
      const parts = [];
      tree.nodes.forEach(function (node) {
        const cls = 'status-' + node.statusName + (node.active ? ' active' : '');
        parts.push(
          '<div class="' +
            cls +
            '">' +
            (node.active ? '▶ ' : '&nbsp;&nbsp;') +
            escapeHtml(node.path) +
            ' <span class="meta">' +
            node.statusName +
            '</span></div>'
        );
      });
      el.innerHTML = parts.join('');
      if (metaEl) {
        metaEl.textContent = 'tick ' + tree.tick + ' · ' + tree.tickMs.toFixed(2) + ' ms · active ' + tree.activeCount;
      }
    },
  };
}
