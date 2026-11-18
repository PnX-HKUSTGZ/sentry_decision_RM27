# 2027 命名点

> 状态：名字与语义已冻结；坐标待 web 标定工具写入 `config/maps/RMUC27.yaml`。
> 关联：[TASK_CATALOG.md](TASK_CATALOG.md)、[STRATEGY_LAYER.md](STRATEGY_LAYER.md)。
> 范围：RMUC 2027，哨兵决策端。

## 1. 原则

- 所有 Task 只接受命名点，不直接传坐标；C++ 常量见 `src/sentry_decision_core/include/sentry_decision_core/points_2027.hpp`。
- 命名点按**己方视角**固定：`own_*` 在己方半场，`enemy_*` 在对方半场。
- Task 层只看到 `own` / `enemy`，不区分红蓝。
- 2027 裁判事件位是 R/B；只在 IO / Belief 层用 `own_color` 映射一次，不进入 `TaskPlan` / `TaskState` / 命名点。
- `own_base_supply`、`own_outpost_buff` 等区域点的第 4 位是区域半径；打击点半径填 0 或省略。
- 导航到达半径由 `Navigate.arrive_radius` 决定（默认 0.2 m），不复用区域半径。

## 2. 点位清单（11 个）

| 名称 | 类别 | 语义 |
|---|---|---|
| `own_base_supply` | supply | 己方基地补给区增益点；`Heal` / `Resource` 使用 |
| `own_fort` | fort | 己方堡垒增益点 |
| `enemy_fort` | fort | 对方堡垒增益点 |
| `own_outpost_buff` | outpost_buff | 己方前哨站正下方增益点；己方完全控制时可用于 `Heal` / `Resource` |
| `enemy_outpost_buff` | outpost_buff | 对方前哨站正下方增益点；己方完全控制时可用于 `Heal` / `Resource` |
| `own_outpost_front` | strike | 己方前哨站前方打击点：靠近前哨站、朝中央高地一侧、不在增益点内 |
| `enemy_outpost_front` | strike | 对方前哨站前方打击点：靠近前哨站、朝中央高地一侧、不在增益点内 |
| `own_outpost_side` | strike_highland | 己方前哨站侧方打击点：同侧高地增益点内，较近但危险 |
| `enemy_outpost_side` | strike_highland | 对方前哨站侧方打击点：同侧高地增益点内，较近但危险 |
| `own_outpost_opposite` | strike_highland | 己方前哨站对面打击点：对侧高地增益点内，较远、较安全 |
| `enemy_outpost_opposite` | strike_highland | 对方前哨站对面打击点：对侧高地增益点内，较远、较安全 |

> `side` 和 `opposite` 都在高地增益点内，享有 25% 防御增益；`front` 不在增益点内。
> “同侧 / 对侧”以中央高地为参照：同侧 = 靠近该前哨站的一侧高地，对侧 = 跨过中央高地的另一侧高地。

## 3. 地图配置格式

沿用当前地图格式：`[x, y, yaw, radius]`，`radius` 可省略。

```yaml
frame_id: map
points:
  own_base_supply:      [x, y, yaw, radius]
  own_fort:             [x, y, yaw, radius]
  enemy_fort:           [x, y, yaw, radius]
  own_outpost_buff:     [x, y, yaw, radius]
  enemy_outpost_buff:   [x, y, yaw, radius]
  own_outpost_front:    [x, y, yaw]
  enemy_outpost_front:  [x, y, yaw]
  own_outpost_side:     [x, y, yaw, radius]
  enemy_outpost_side:   [x, y, yaw, radius]
  own_outpost_opposite: [x, y, yaw, radius]
  enemy_outpost_opposite: [x, y, yaw, radius]
```

## 4. 使用约定

| Task | 允许的点 |
|---|---|
| `Navigate` | 以上任意 11 个点 |
| `Heal` | `own_base_supply` 或 `own_outpost_buff` |
| `Resource` | `own_base_supply` 或 `own_outpost_buff`（后者要求己方完全控制） |

`Heal` / `Resource` 通过点名区分补给区类型，不额外传 `zone_kind`。
新增补给点时必须同时更新 `TASK_CATALOG.md` 与本表。

## 5. 待办

- 用 web 标定工具填入 11 个点的坐标、yaw 与区域半径。
- `RosIoNode` 参数 `own_color`（red/blue）用于把 2027 R/B 事件位映射为 own/enemy。
- 清理 RMUC26 遗留点（`home` / `patrol_*` / `highland_*` 等）与 2027 配置的引用关系。
