# 2027 命名点

> 状态：名字与语义已冻结；坐标待 web 标定工具写入 `config/maps/RMUC27.yaml`。
> 关联：[TASK_CATALOG.md](TASK_CATALOG.md)、[STRATEGY_LAYER.md](STRATEGY_LAYER.md)。
> 范围：RMUC 2027，哨兵决策端。

## 1. 原则

- 所有 Task 只接受命名点，不直接传坐标；C++ 常量见 `src/sentry_decision_core/include/sentry_decision_core/points_2027.hpp`。
- 命名点按**己方视角**固定：`own_*` 在己方半场，`enemy_*` 在对方半场。
- Task 层只看到 `own` / `enemy`，不区分红蓝。
- 2027 裁判事件位是 R/B；只在 IO / Belief 层用 `own_color` 映射一次，不进入 `TaskPlan` / `TaskState` / 命名点。
- `own_base_supply`、`own_outpost_buff` 等区域点的第 4 位是区域半径；`front` 等纯打击点填 0 或省略；`side` / `opposite` 同时是高地增益点，填 0.5。
- 导航到达半径由 `Navigate.arrive_radius` 决定（默认 0.2 m），不复用区域半径。

## 2. 点位清单（14 个）

| 名称 | 类别 | 语义 |
|---|---|---|
| `own_base_supply` | supply | 己方基地补给区增益点；`Heal` / `Resource` 使用 |
| `own_fort` | fort | 己方堡垒增益点 |
| `enemy_fort` | fort | 对方堡垒增益点 |
| `own_outpost_buff` | outpost_buff | 己方前哨站正下方增益点；己方完全控制时可用于 `Heal` / `Resource` |
| `enemy_outpost_buff` | outpost_buff | 对方前哨站正下方增益点；己方完全控制且前哨站存活时可用于 `Heal` / `Resource` |
| `own_outpost_front` | strike | 己方前哨站前方打击点：靠近前哨站、朝中央高地一侧、不在增益点内 |
| `enemy_outpost_front` | strike | 对方前哨站前方打击点：靠近前哨站、朝中央高地一侧、不在增益点内 |
| `own_outpost_side` | strike_highland | 己方前哨站侧方打击点：同侧高地增益点内，较近但危险；半径 0.5 m |
| `enemy_outpost_side` | strike_highland | 对方前哨站侧方打击点：同侧高地增益点内，较近但危险；半径 0.5 m |
| `own_outpost_opposite` | strike_highland | 己方前哨站对面打击点：对侧高地增益点内，较远、较安全；半径 0.5 m |
| `enemy_outpost_opposite` | strike_highland | 对方前哨站对面打击点：对侧高地增益点内，较远、较安全；半径 0.5 m |
| `highland_area_center` | highland_area | 中央高地区域；用于判断哨兵是否处于高地回血范围，半径 5 m |
| `highland_area_enemy` | highland_area | 敌方高地区域；与中央高地一起决定是否去敌方前哨站增益点回血，半径 3 m |
| `highland_area_our` | highland_area | 己方高地区域；与中央高地一起决定是否去己方前哨站增益点回血，半径 3 m |

> `side` 和 `opposite` 都在高地增益点内，享有 25% 防御增益，并可作为前哨阵地的占点输入；`front` 不在增益点内。
> 前哨站增益点只在前哨站存活且己方完全控制时有效；被击毁或失去完全控制后，应改用中央高地增益点。
> 仿真中己方高地为 `own_outpost_side` + `enemy_outpost_opposite`，敌方高地为 `enemy_outpost_side` + `own_outpost_opposite`，半径均 0.5 m。
> 四个 `*_outpost_front/side/opposite` 都是打击点，不需要区域半径。
> “同侧 / 对侧”以中央高地为参照：同侧 = 靠近该前哨站的一侧高地，对侧 = 跨过中央高地的另一侧高地。
> `highland_area_*` 只用于 `Heal` 回血点判断：中心区域 + 敌方区域 → 可去敌方前哨增益点；中心区域 + 己方区域 → 可去己方前哨增益点；否则回基地。

## 3. 标定底图

标定工具已切换为 RMUC 2027 底图：

```text
src/sentry_decision_viz/web/assets/field-rmuc2027.png
```

来源是导航仓库最新版：

```text
src/pnx-launch/launch_bringup/map/simulation/RMUC2027.pgm
src/pnx-launch/launch_bringup/map/simulation/RMUC2027.yaml
```

当前参数：

- 图像尺寸：583 × 300 px
- `resolution`：0.05 m/px
- `origin`：`[-14.6, -5.86, 0]`

导航地图更新后，重新生成 PNG：

```bash
python3 - <<'PY'
from PIL import Image
src = "../sentry_navgation_RM27/src/pnx-launch/launch_bringup/map/simulation/RMUC2027.pgm"
dst = "src/sentry_decision_viz/web/assets/field-rmuc2027.png"
Image.open(src).save(dst)
PY
```

`pcd` 不是标定底图的必需文件；它是导航侧的点云先验，标定只用 `.pgm` / PNG。

标定工具的 2027 点位默认从本地存储 `sentry.fieldPoints.rmuc2027.v1` 读取；旧 2026
点位存在旧键中，不会自动载入。需要时点「重置为默认」，或从文本导入 2027 YAML。

## 4. 地图配置格式

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
  own_outpost_side:     [x, y, yaw]
  enemy_outpost_side:   [x, y, yaw]
  own_outpost_opposite: [x, y, yaw]
  enemy_outpost_opposite: [x, y, yaw]
  highland_area_center:  [x, y, yaw, radius]
  highland_area_enemy:   [x, y, yaw, radius]
  highland_area_our:     [x, y, yaw, radius]
```

## 5. 使用约定

| Task | 允许的点 |
|---|---|
| `Navigate` | 以上任意 14 个点 |
| `Heal` | `own_base_supply`、`own_outpost_buff` 或 `enemy_outpost_buff`（后两者要求己方完全控制） |
| `Resource` | `own_base_supply`、`own_outpost_buff` 或 `enemy_outpost_buff`（后两者要求己方完全控制且前哨站存活） |

`Heal` / `Resource` 通过点名区分补给区类型，不额外传 `zone_kind`。
新增补给点时必须同时更新 `TASK_CATALOG.md` 与本表。

## 6. 当前状态

- `config/maps/RMUC27.yaml` 已完成 14 个命名点标定，并作为 `config/profiles.yaml` 的默认地图。
- `config/sim.yaml` 已同步使用 RMUC27 的补给区 / 增益点。
- `RosIoNode` 参数 `own_color`（red/blue）用于把 2027 R/B 事件位映射为 own/enemy。
- RMUC26 地图仅保留为历史配置，不再作为默认入口。
