// 场地标定工具：在 RMUC 底图上点选 / 拖拽命名点，编辑并导出 config/maps 用的 YAML。
// 静态页，无 ROS 依赖；点位存 localStorage，也可导出 / 导入 YAML 文本。
import {
  DEFAULT_POINTS,
  FIELD,
  fieldExtent,
  imageToWorld,
  worldToImage,
} from './field.js';

const STORAGE_KEY = 'sentry.fieldPoints.v1';
const SCALE = 2; // 画布 = 底图 * 2
const PALETTE = ['#4aa3ff', '#49c46a', '#e0a44a', '#e05a5a', '#b07cff', '#4ad0c4', '#ff8fb0', '#8bd450', '#ffd24a'];

const canvas = document.getElementById('field');
const ctx = canvas.getContext('2d');
const listEl = document.getElementById('list');
const ioEl = document.getElementById('io');
const cursorEl = document.getElementById('cursor');
const countEl = document.getElementById('count');

const image = new Image();
image.src = FIELD.image;
image.onload = function () { draw(); };

let points = loadPoints();
let selected = 0;
let dragging = null;
const rowRefs = [];

function clone(list) {
  return list.map(function (p) {
    return { name: p.name, x: p.x, y: p.y, yaw: p.yaw || 0 };
  });
}

function loadPoints() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length) {
        return clone(parsed);
      }
    }
  } catch (error) {
    // 本地数据损坏时回退默认值
  }
  return clone(DEFAULT_POINTS);
}

function savePoints() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(points));
  } catch (error) {
    // localStorage 不可用时忽略
  }
}

function colorFor(index) {
  return PALETTE[index % PALETTE.length];
}

function num(value) {
  const n = Number(value);
  return (isFinite(n) ? n : 0).toFixed(2);
}

function draw() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = '#0b1015';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  if (image.complete && image.naturalWidth) {
    ctx.drawImage(image, 0, 0, FIELD.widthPx * SCALE, FIELD.heightPx * SCALE);
  }
  drawGrid();
  points.forEach(function (point, index) {
    drawMarker(point, index);
  });
}

function drawGrid() {
  const extent = fieldExtent();
  ctx.strokeStyle = 'rgba(74, 163, 255, 0.18)';
  ctx.fillStyle = '#5f7285';
  ctx.font = '11px monospace';
  ctx.lineWidth = 1;
  let gx = Math.ceil(extent.minX / 5) * 5;
  for (; gx <= extent.maxX; gx += 5) {
    const a = worldToImage({ x: gx, y: extent.minY }, FIELD, SCALE);
    const b = worldToImage({ x: gx, y: extent.maxY }, FIELD, SCALE);
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.stroke();
    ctx.fillText('x=' + gx, a.x + 2, canvas.height - 4);
  }
  let gy = Math.ceil(extent.minY / 5) * 5;
  for (; gy <= extent.maxY; gy += 5) {
    const a = worldToImage({ x: extent.minX, y: gy }, FIELD, SCALE);
    const b = worldToImage({ x: extent.maxX, y: gy }, FIELD, SCALE);
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.stroke();
    ctx.fillText('y=' + gy, 4, a.y - 2);
  }
}

function drawMarker(point, index) {
  const p = worldToImage(point, FIELD, SCALE);
  const active = index === selected;
  ctx.beginPath();
  ctx.arc(p.x, p.y, active ? 7 : 5, 0, Math.PI * 2);
  ctx.fillStyle = colorFor(index);
  ctx.fill();
  if (active) {
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 2;
    ctx.stroke();
  }
  ctx.fillStyle = active ? '#ffffff' : '#dfe7ef';
  ctx.font = (active ? 'bold ' : '') + '13px monospace';
  ctx.fillText(point.name, p.x + 9, p.y - 6);
}

function renderList() {
  rowRefs.length = 0;
  listEl.textContent = '';
  points.forEach(function (point, index) {
    const row = document.createElement('div');
    row.className = 'pt-row' + (index === selected ? ' selected' : '');
    row.addEventListener('click', function () { select(index); });

    const dot = document.createElement('span');
    dot.className = 'pt-dot';
    dot.style.background = colorFor(index);
    row.appendChild(dot);

    const name = document.createElement('input');
    name.className = 'pt-name';
    name.value = point.name;
    name.addEventListener('input', function () { point.name = name.value; draw(); savePoints(); });
    name.addEventListener('focus', function () { select(index); });
    row.appendChild(name);

    const inputs = {};
    ['x', 'y', 'yaw'].forEach(function (axis) {
      const input = document.createElement('input');
      input.className = 'pt-num';
      input.title = axis;
      input.value = num(point[axis]);
      input.addEventListener('input', function () {
        const value = Number(input.value);
        if (isFinite(value)) {
          point[axis] = value;
          draw();
          savePoints();
        }
      });
      input.addEventListener('focus', function () { select(index); });
      row.appendChild(input);
      inputs[axis] = input;
    });

    const del = document.createElement('button');
    del.className = 'pt-del';
    del.textContent = '×';
    del.title = '删除该点';
    del.addEventListener('click', function (event) {
      event.stopPropagation();
      points.splice(index, 1);
      if (selected >= points.length) {
        selected = points.length - 1;
      }
      if (selected < 0) {
        selected = 0;
      }
      renderList();
      draw();
      savePoints();
    });
    row.appendChild(del);

    listEl.appendChild(row);
    rowRefs.push({ row: row, name: name, x: inputs.x, y: inputs.y, yaw: inputs.yaw });
  });
  countEl.textContent = '共 ' + points.length + ' 个';
}

function select(index) {
  if (index < 0 || index >= points.length || index === selected) {
    return;
  }
  if (rowRefs[selected]) {
    rowRefs[selected].row.classList.remove('selected');
  }
  selected = index;
  if (rowRefs[selected]) {
    rowRefs[selected].row.classList.add('selected');
  }
  draw();
}

function syncInputs() {
  const ref = rowRefs[selected];
  if (!ref) {
    return;
  }
  ref.x.value = num(points[selected].x);
  ref.y.value = num(points[selected].y);
  ref.yaw.value = num(points[selected].yaw);
}

function canvasPos(event) {
  const rect = canvas.getBoundingClientRect();
  return {
    x: (event.clientX - rect.left) * (canvas.width / rect.width),
    y: (event.clientY - rect.top) * (canvas.height / rect.height),
  };
}

function hitTest(pos) {
  for (let i = points.length - 1; i >= 0; i -= 1) {
    const p = worldToImage(points[i], FIELD, SCALE);
    const dx = p.x - pos.x;
    const dy = p.y - pos.y;
    if (dx * dx + dy * dy <= 120) {
      return i;
    }
  }
  return -1;
}

function moveSelectedTo(pos) {
  const world = imageToWorld(pos.x, pos.y, FIELD, SCALE);
  points[selected].x = Math.round(world.x * 100) / 100;
  points[selected].y = Math.round(world.y * 100) / 100;
  syncInputs();
  draw();
  savePoints();
}

canvas.addEventListener('pointerdown', function (event) {
  const pos = canvasPos(event);
  const hit = hitTest(pos);
  if (hit >= 0) {
    select(hit);
    dragging = hit;
  } else if (points.length > 0) {
    dragging = selected;
    moveSelectedTo(pos);
  }
  try { canvas.setPointerCapture(event.pointerId); } catch (error) { /* 忽略 */ }
});

canvas.addEventListener('pointermove', function (event) {
  const pos = canvasPos(event);
  const world = imageToWorld(pos.x, pos.y, FIELD, SCALE);
  cursorEl.textContent = 'x=' + world.x.toFixed(2) + '  y=' + world.y.toFixed(2);
  if (dragging !== null) {
    moveSelectedTo(pos);
  }
});

canvas.addEventListener('pointerup', function (event) {
  dragging = null;
  try { canvas.releasePointerCapture(event.pointerId); } catch (error) { /* 忽略 */ }
});

function exportYaml() {
  let width = 0;
  points.forEach(function (p) {
    if (p.name.length > width) {
      width = p.name.length;
    }
  });
  const lines = [
    '# RMUC 地图点位（map 坐标系，单位米）。',
    '# 由网页面板「场地标定」生成：/calibrate.html',
    'frame_id: map',
    'points:',
  ];
  points.forEach(function (p) {
    const pad = ' '.repeat(width - p.name.length + 1);
    lines.push('  ' + p.name + ':' + pad + '[' + num(p.x) + ', ' + num(p.y) + ', ' + num(p.yaw) + ']');
  });
  return lines.join('\n') + '\n';
}

function parseYaml(text) {
  const found = [];
  text.split('\n').forEach(function (line) {
    const colon = line.indexOf(':');
    if (colon < 0) {
      return;
    }
    const open = line.indexOf('[', colon);
    const close = line.indexOf(']', open + 1);
    if (open < 0 || close < 0) {
      return;
    }
    const name = line.slice(0, colon).trim();
    if (!name || name === 'frame_id' || name === 'points') {
      return;
    }
    const parts = line
      .slice(open + 1, close)
      .split(',')
      .map(function (item) { return Number(item.trim()); });
    if (parts.length < 2 || !isFinite(parts[0]) || !isFinite(parts[1])) {
      return;
    }
    found.push({
      name: name,
      x: parts[0],
      y: parts[1],
      yaw: parts.length > 2 && isFinite(parts[2]) ? parts[2] : 0,
    });
  });
  return found;
}

document.getElementById('add').addEventListener('click', function () {
  const extent = fieldExtent();
  points.push({
    name: 'point_' + (points.length + 1),
    x: Math.round(((extent.minX + extent.maxX) / 2) * 100) / 100,
    y: Math.round(((extent.minY + extent.maxY) / 2) * 100) / 100,
    yaw: 0,
  });
  selected = points.length - 1;
  renderList();
  draw();
  savePoints();
});

document.getElementById('reset').addEventListener('click', function () {
  points = clone(DEFAULT_POINTS);
  selected = 0;
  renderList();
  draw();
  savePoints();
});

document.getElementById('export').addEventListener('click', function () {
  ioEl.value = exportYaml();
});

document.getElementById('copy').addEventListener('click', function () {
  const text = ioEl.value || exportYaml();
  ioEl.value = text;
  if (navigator.clipboard) {
    navigator.clipboard.writeText(text).catch(function () { /* 忽略 */ });
  }
});

document.getElementById('import').addEventListener('click', function () {
  const parsed = parseYaml(ioEl.value);
  if (parsed.length) {
    points = parsed;
    selected = 0;
    renderList();
    draw();
    savePoints();
  }
});

document.getElementById('clear-io').addEventListener('click', function () {
  ioEl.value = '';
});

renderList();
draw();
