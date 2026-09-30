import { worldToCanvas } from './format.js';
import { FIELD } from './field.js';

// 场地底图：RMUC2026（由导航仓库 RMUC2026.pgm 转换）。世界坐标 -> 像素映射见 field.js。
// 半场：RMUC 长边为 x 轴，己方在 x<0（左）。
const fieldImage = new Image();
fieldImage.src = FIELD.image;
let lastCanvas = null;
let lastModel = null;
fieldImage.onload = function () {
  if (lastCanvas && lastModel) {
    drawBattlefield(lastCanvas, lastModel);
  }
};

function extentRect(extent, width, height) {
  const a = worldToCanvas({ x: extent.minX, y: extent.maxY }, extent, width, height);
  const b = worldToCanvas({ x: extent.maxX, y: extent.minY }, extent, width, height);
  return {
    x: Math.min(a.x, b.x),
    y: Math.min(a.y, b.y),
    width: Math.abs(b.x - a.x),
    height: Math.abs(b.y - a.y),
  };
}

function label(ctx, text, x, y, align) {
  ctx.save();
  ctx.font = '13px monospace';
  ctx.textAlign = align || 'left';
  ctx.lineWidth = 3;
  ctx.strokeStyle = 'rgba(238, 242, 246, 0.9)';
  ctx.strokeText(text, x, y);
  ctx.fillStyle = '#1d2833';
  ctx.fillText(text, x, y);
  ctx.restore();
}

function drawField(ctx, extent, width, height) {
  ctx.fillStyle = '#0b1015';
  ctx.fillRect(0, 0, width, height);
  const rect = extentRect(extent, width, height);
  if (fieldImage.complete && fieldImage.naturalWidth) {
    ctx.drawImage(fieldImage, rect.x, rect.y, rect.width, rect.height);
  } else {
    ctx.fillStyle = '#8b9bab';
    ctx.font = '14px monospace';
    ctx.fillText('加载场地地图…', rect.x + 10, rect.y + 20);
  }
  ctx.strokeStyle = '#4a5d70';
  ctx.lineWidth = 2;
  ctx.strokeRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2);
  label(ctx, '己方半场（x<0）', rect.x + 8, rect.y + rect.height - 8, 'left');
  label(ctx, '敌方半场（x>0）', rect.x + rect.width - 8, rect.y + rect.height - 8, 'right');
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
  lastCanvas = canvas;
  lastModel = model;
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
    label(ctx, '等待定位 /odom …', 10, height - 12, 'left');
  }
  // mode 放右上角，避免与半场标注重叠。
  label(ctx, 'mode: ' + model.mode, width - 10, 20, 'right');
}
