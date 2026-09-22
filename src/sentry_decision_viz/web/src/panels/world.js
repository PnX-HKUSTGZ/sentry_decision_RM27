import { tacticalModeName } from '../format.js';

function row(label, value) {
  return '<tr><th>' + label + '</th><td>' + value + '</td></tr>';
}

// 世界状态面板：WorldState + 当拍 DecisionOutput 关键字段。
export function createWorldPanel(el) {
  return {
    render: function (state) {
      const w = state.world;
      const d = state.decision;
      if (!w) {
        el.textContent = '等待 /decision/world_state …';
        return;
      }
      // 覆盖前的真实世界（list_state 轮询）；有覆盖时提示决策视图被钉住。
      const snapshot = state.listState || {};
      const raw = snapshot.raw_world;
      const overrides = snapshot.world_overrides || {};
      const overrideNames = Object.keys(overrides);
      let html = '<table>';
      html += row('模式', d ? d.modeName : '-');
      html += row('期望 / 当前姿态', (d ? d.stanceName : '-') + ' / ' + w.stanceName);
      html += row('血量 / 弹量', w.selfHp + ' / ' + w.selfAmmo);
      html += row('金币', w.coins);
      if (raw) {
        html += row(
          '原始 血/弹/金',
          raw.self_hp + ' / ' + raw.self_ammo + ' / ' + raw.coins +
            (overrideNames.length ? '（决策覆盖生效）' : '')
        );
      }
      html += row('己方基地 / 前哨', w.baseHp + ' / ' + w.ourOutpostHp);
      html += row('敌方前哨 / 基地', w.enemyOutpostHp + ' / ' + w.enemyBaseHp);
      html += row('剩余时间', w.gameTime + ' s');
      html += row('比赛阶段', w.gameStatus);
      html += row('位姿', w.posX.toFixed(2) + ', ' + w.posY.toFixed(2) + ' @ ' + (w.yaw || 0).toFixed(2));
      html += row('导航目标', w.hasNavGoal ? w.navGoalX.toFixed(2) + ', ' + w.navGoalY.toFixed(2) : '无');
      html += row('有效', (w.refereeValid ? 'referee ' : '') + (w.selfValid ? 'odom ' : '') + (w.enemyValid ? 'enemy' : ''));
      html += row('敌方', w.enemyCount + ' 个 / 锁定 ' + (w.hasEnemy ? '是' : '否'));
      html += '</table>';
      el.innerHTML = html;
    },
  };
}
