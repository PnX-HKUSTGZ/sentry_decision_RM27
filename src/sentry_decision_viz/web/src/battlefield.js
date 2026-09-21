import { worldToCanvas } from './format.js';

function grid(ctx, extent, width, height) {
  ctx.strokeStyle = '#22303c';
  ctx.lineWidth = 1;
  for (let gx = Math.ceil(extent.minX); gx <= extent.maxX; gx += 1) {
    const p = worldToCanvas({ x: gx, y: 0 }, extent, width, height);
    ctx.beginPath();
    ctx.moveTo(p.x, 0);
    ctx.lineTo(p.x, height);
    ctx.stroke();
  }
  for (let gy = Math.ceil(extent.minY); gy <= extent.maxY; gy += 1) {
    const p = worldToCanvas({ x: 0, y: gy }, extent, width, height);
    ctx.beginPath();
    ctx.moveTo(0, p.y);
    ctx.lineTo(width, p.y);
    ctx.stroke();
  }
  const origin = worldToCanvas({ x: 0, y: 0 }, extent, width, height);
  ctx.strokeStyle = '#3a4b5c';
  ctx.beginPath();
  ctx.moveTo(origin.x, 0);
  ctx.lineTo(origin.x, height);
  ctx.moveTo(0, origin.y);
  ctx.lineTo(width, origin.y);
  ctx.stroke();
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
  grid(ctx, extent, width, height);
  if (model.goal) {
    drawGoal(ctx, model.goal, extent, width, height);
  }
  if (model.enemy) {
    drawEnemy(ctx, model.enemy, extent, width, height);
  }
  if (model.self) {
    drawSelf(ctx, model.self, extent, width, height);
  }
  ctx.fillStyle = '#8b9bab';
  ctx.font = '14px monospace';
  ctx.fillText('mode: ' + model.mode, 10, 20);
}
