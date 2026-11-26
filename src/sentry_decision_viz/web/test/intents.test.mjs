// 纯逻辑单测：资源与动作面板的 TaskSnapshot / TaskResult 渲染（无浏览器）。
import { createIntentsPanel } from '../src/panels/intents.js';

let failures = 0;

function check(ok, message) {
  if (!ok) {
    console.error('FAIL: ' + message);
    failures += 1;
  }
}

const el = { innerHTML: '', textContent: '' };
const panel = createIntentsPanel(el);

panel.render({ listState: null });
check(el.textContent.includes('等待'), '等待占位');

panel.render({
  listState: {
    safety_emergency: false,
    task: {
      id: 'Heal',
      status: 1,
      state: 5,
      phase: 'wait',
      detail: '回血中',
      progress: 0.5,
      elapsed_ms: 1500,
      params: 'heal_point=own_base_supply, claim_free_ammo=true',
      metrics: 'hp=300.000; distance=0.100',
    },
    previous: {
      id: 'Navigate',
      status: 2,
      reason: '到达 own_base_supply',
      elapsed_ms: 9000,
      metrics: 'distance_remaining=0.100',
    },
    resource: {
      id: 'Resource',
      status: 1,
      phase: 'exchange',
      detail: '等待到账',
      params: 'exchange_point=own_base_supply, amount=100',
      metrics: 'target=100.000',
    },
  },
});

check(el.innerHTML.includes('Heal'), '当前任务');
check(el.innerHTML.includes('running / wait'), '任务状态与阶段');
check(el.innerHTML.includes('1.5 s'), '已进行时间');
check(!el.innerHTML.includes('进度'), '不显示进度行');
check(el.innerHTML.includes('section-start'), '上一次任务分隔线');
check(el.innerHTML.includes('heal_point=own_base_supply'), '参数');
check(el.innerHTML.includes('hp=300.000'), '指标');
check(el.innerHTML.includes('Navigate'), '上一次任务');
check(el.innerHTML.includes('success'), '上一次结果');
check(el.innerHTML.includes('到达 own_base_supply'), '上一次原因');
check(el.innerHTML.includes('9.0 s'), '上一次用时');
check(el.innerHTML.includes('辅助兑换'), '辅助兑换行');
check(el.innerHTML.includes('exchange_point=own_base_supply'), '辅助兑换参数');
check(el.innerHTML.includes('target=100.000'), '辅助兑换指标');
check(el.innerHTML.includes('否'), '急停状态');

if (failures !== 0) {
  console.error(failures + ' check(s) failed');
  process.exit(1);
}
console.log('intents.test passed');
