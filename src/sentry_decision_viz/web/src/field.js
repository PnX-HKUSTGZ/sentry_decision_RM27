// RMUC 2026 场地底图与坐标换算。
// 数据来源：导航仓库 pb2025_nav_bringup/map/simulation/RMUC2026.{pgm,yaml}
// （resolution 0.05 m/px，origin [-14.6, -5.86]）。本文件为纯函数，可在 node 下单测。

export const FIELD = {
  image: 'assets/field-rmuc2026.png',
  resolution: 0.05, // m/px
  origin: { x: -14.6, y: -5.86 },
  widthPx: 583,
  heightPx: 300,
};

// 场地覆盖的世界坐标范围。
export function fieldExtent(field) {
  const f = field || FIELD;
  return {
    minX: f.origin.x,
    maxX: f.origin.x + f.widthPx * f.resolution,
    minY: f.origin.y,
    maxY: f.origin.y + f.heightPx * f.resolution,
  };
}

// 世界坐标 -> 图像像素（scale 为图像放大倍数）。
// 占据栅格图像首行对应 maxY，因此 y 轴需要翻转。
export function worldToImage(point, field, scale) {
  const f = field || FIELD;
  const s = scale || 1;
  return {
    x: ((point.x - f.origin.x) / f.resolution) * s,
    y: ((f.origin.y + f.heightPx * f.resolution - point.y) / f.resolution) * s,
  };
}

export function imageToWorld(px, py, field, scale) {
  const f = field || FIELD;
  const s = scale || 1;
  return {
    x: f.origin.x + (px / s) * f.resolution,
    y: f.origin.y + (f.heightPx - py / s) * f.resolution,
  };
}

// 标定工具的初始点位：空序列，避免把示例点误当成真实地图配置。
// 真实点位由用户手动维护在 config/maps/*.yaml，并通过「添加点 / 从文本导入」建立。
export const DEFAULT_POINTS = [];
