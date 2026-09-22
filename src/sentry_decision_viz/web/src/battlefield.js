import { worldToCanvas } from './format.js';

// 场地底图：网格 + 边界 + 中线 / 中圈 + 半场标注。
// 目前只做示意（未接入真实地图），目的是让俯视图不再空白、便于判断坐标方向。
function drawField(ctx, extent, width, height) {
  const a = worldToCanvas({ x: extent.minX, y: extent.minY }, extent, width, height);
  const b = worldToCanvas({ x: extent.maxX, y: extent.maxY }, extent, width, height);
  const left = Math.min(a.x, b.x);
  const right = Math.max(a.x, b.x);
  const top = Math.min(a.y, b.y);
  const bottom = Math.max(a.y, b.y);

  ctx.fillStyle = '#0d151c';
  ctx.fillRect(left, top, right - left, bottom - top);

  // 1m 网格
  ctx.strokeStyle = '#1d2833';
  ctx.lineWidth = 1;
  for (let gx = Math.ceil(extent.minX); gx <= extent.maxX; gx += 1) {
    const p = worldToCanvas({ x: gx, y: 0 }, extent, width, height);
    ctx.beginPath();
    ctx.moveTo(p.x, top);
    ctx.lineTo(p.x, bottom);
    ctx.stroke();
  }
  for (let gy = Math.ceil(extent.minY); gy <= extent.maxY; gy += 1) {
    const p = worldToCanvas({ x: 0, y: gy }, extent, width, height);
    ctx.beginPath();
    ctx.moveTo(left, p.y);
    ctx.lineTo(right, p.y);
    ctx.stroke();
  }

  // 中线（世界 y = 0）与中圈
  const center = worldToCanvas({ x: 0, y: 0 }, extent, width, height);
  ctx.strokeStyle = '#3a4b5c';
  ctx.lineWidth = 1.5;
  ctx.setLineDash([7, 6]);
  ctx.beginPath();
  ctx.moveTo(left, center.y);
  ctx.lineTo(right, center.y);
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.beginPath();
  ctx.arc(center.x, center.y, 26, 0, Math.PI * 2);
  ctx.stroke();

  // 边界
  ctx.strokeStyle = '#4a5d70';
  ctx.lineWidth = 2;
  ctx.strokeRect(left + 1, top + 1, right - left - 2, bottom - top - 2);

  // 半场标注
  ctx.fillStyle = '#5f7285';
  ctx.font = '13px monospace';
  ctx.fillText('敌方半场', left + 8, top + 18);
  ctx.fillText('己方半场', left + 8, bottom - 8);
}

function drawGoal(ctx, point, extent, width, height) {
  const p = worldToCanvas(point, extent, width, height);
  ctx.strokeStyle = '#49c46a';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(p.x, p.y, 7, 0, Math.PI * 2);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(p.x - 10, p.y);
  ctx.lineTo(p.x + 10, p.y);
  ctx.moveTo(p.x, p.y - 10);
  ctx.lineTo(p.x, p.y + 10);
  ctx.stroke();
}

function drawEnemy(ctx, point, extent, width, height) {
  const p = worldToCanvas(point, extent, width, height);
  ctx.fillStyle = '#e05a5a';
  ctx.fillRect(p.x - 6, p.y - 6, 12, 12);
}

function drawSelf(ctx, self, extent, width, height) {
  const p = worldToCanvas(self, extent, width, height);
  const yaw = self.yaw || 0;
  ctx.save();
  ctx.translate(p.x, p.y);
  // 地图 yaw 为逆时针，canvas y 轴向下，取反向。
  ctx.rotate(-yaw);
  ctx.fillStyle = '#4aa3ff';
  ctx.beginPath();
  ctx.moveTo(11, 0);
  ctx.lineTo(-7, 7);
  ctx.lineTo(-7, -7);
  ctx.closePath();
  ctx.fill();
  ctx.restore();
}

export function drawBattlefield(canvas, model) {
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  const extent = model.extent;
  ctx.clearRect(0, 0, width, height);
  drawField(ctx, extent, width, height);
  if (model.goal) {
    drawGoal(ctx, model.goal, extent, width, height);
  }
  if (model.enemy) {
    drawEnemy(ctx, model.enemy, extent, width, height);
  }
  if (model.self) {
    drawSelf(ctx, model.self, extent, width, height);
  } else {
    ctx.fillStyle = '#8b9bab';
    ctx.font = '14px monospace';
    ctx.fillText('等待定位 /odom …', 10, height - 12);
  }
  ctx.fillStyle = '#8b9bab';
  ctx.font = '14px monospace';
  ctx.fillText('mode: ' + model.mode, 10, 20);
}
