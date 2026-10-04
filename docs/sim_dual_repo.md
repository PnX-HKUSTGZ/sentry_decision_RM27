# 双仓库联调：导航接入决策本地仿真

> 状态：实施中。决策分支 `v0.4.0`，导航仓库保持 `dock/jazzy` 不改代码。
> 本文是「决策本地仿真 ↔ 真实导航栈」联调的设计与接口单一事实来源；实施进度见 `docs/NOTE.md`。

## 1. 目标与边界

- **保持信念层的裁判仿真输入不变**：`/sentry/game_info` 等 5 条上行仍由 `referee_sim_node` 提供。
- **替换导航互动为真实逻辑**：决策下发的导航目标（`/navigate_to_pose`）、底盘速度（`/cmd_vel`）、
  以及导航回传的位姿，全部改由导航仓库（Gazebo + Point-LIO + Nav2）提供。
- `tools/demo.sh` 一键运行，**兼容两种模式**：
  - 独立仿真（现有伪导航，默认，行为不变）；
  - 双仓库联调（真实导航）。
- 真实位姿实时反馈在 web 面板；导航可静默（`gui:=false use_rviz:=false`）。
- **不做暂停**：暂停会引入与时间基准 / 恢复相关的额外状态，sim2real 风险高于收益（已移除现有 sim 暂停）。
- **不纳入 CI**：双仓库 E2E 依赖 Gazebo + Point-LIO，仅在本地脚本运行。

## 2. 接口契约

| 方向 | 决策侧 | 导航侧 | 坐标 / 类型 |
| --- | --- | --- | --- |
| 裁判上行 | `RosIoNode` 订阅 | `referee_sim_node` 发布 | `/sentry/game_info` 等 5 条（`sentry_interfaces`） |
| 导航目标 | `RosIoNode::send_goal` | Nav2 `/bt_navigator` | action `/navigate_to_pose`，`map` 系 |
| 位姿回传 | `RosIoNode` 订阅 | Point-LIO / loam_interface | `/odometry`（`odom` 系） |
| 坐标变换 | 决策侧 tf2 | `map->odom` TF | `map` 系位姿 = TF(`map->odom`) ∘ odom 位姿 |
| 底盘速度 | `RosIoNode::set_velocity` | `/cmd_vel` → Gazebo | `geometry_msgs/Twist` |
| 决策下行 | `RosIoNode::send_action` | `referee_sim_node` 回 `DecisionAck` | `sentry_interfaces/DecisionCommand` |

导航侧定位由 `relocalization_manager` + `small_gicp` 完成：加载先验点云
`RMUC2026.pcd` 与栅格地图 `RMUC2026.yaml`，匹配成功后发布 `map->odom`。

运行时参数（决策侧 ROS 参数）：

| 参数 | 默认 | 双仓库取值 |
| --- | --- | --- |
| `odom_topic` | `/aft_mapped_to_init` | `/odometry` |
| `odom_frame` | 空（话题已是 map 系） | `odom`（触发 tf2 变换） |
| `map_frame` | 取地图配置 `frame_id` | `map` |
| `navigate_action` | `navigate_to_pose` | `navigate_to_pose` |

> `timeouts.odometry_ms` 在 `config/policies/<POLICY>.yaml` 中设为 500ms：双仓库的 `/odometry`
> 由点云回调驱动（约 10Hz 且带抖动），200ms 会偶发判失效触发急停抖动。

## 3. 关键决策

1. **跨容器网络用自定义 bridge `rm27net`**（Phase 0 实测：topic / action 数据面互通）。
   导航文档中的 `--network host` 方案不采用。
2. **定位走真实重定位（`slam:=False` + `RMUC2026.pcd`）**：
   PCD 放在导航仓库 `pcd/simulation/RMUC2026.pcd`（`.gitignore` 已忽略）；
   容器内把它挂到 `/pcd` 并传 `prior_pcd_file:=/pcd/RMUC2026.pcd`。
   实测 `LOCALIZING -> LOCALIZED`，GICP `mean_error=0.01`，`map->base_footprint` ≈ 出生点，
   map 系目标规划到达成功。**导航仓库无需改代码**。
   - 曾评估并放弃：`slam:=True`（SLAM 以起点为原点建图，无法与绝对地图对齐）；
     无 PCD 时用静态 `map->odom` 代替重定位（纯 odom 会漂）。
3. **位姿坐标系在决策侧变换（方案 A）**：`RosIoNode` 订阅 odom 系话题，用 tf2 `map->odom`
   转成 map 系后再写入 `SelfState`。独立仿真无 TF 时由 `odom_frame` 留空保持原行为。
4. **`decision_node` 支持 `--ros-args` 透传**：topic / action / frame 属启动接线，用 ROS 参数覆盖；
   与 `referee_sim_node` 行为一致。
5. **`referee_sim_node` 解耦导航**：新增 `provide_nav`（默认 `true`）。双仓库时设 `false`：
   不再自建 `/navigate_to_pose` server、不再发 odom，改用 `/decision/world_state` 的 map 系位姿做
   补给区 / 占位判定。
6. **移除仿真暂停**（已完成）。
7. **不纳入 CI**。

## 4. 分步计划

| 步骤 | 内容 | 验收 |
| --- | --- | --- |
| N1 | PCD 落位 + 容器验证真实重定位 | `/map` 正常、`map->base_footprint` ≈ 出生点、map 系目标规划成功 |
| D1 | `decision_node` 支持 `--ros-args` | `--ros-args -p odom_topic:=/x` 正常启动 |
| D2 | 决策 tf2 坐标变换 + 纯逻辑单测 | `odom_frame` 为空行为不变；给出 TF 时输出 map 系 |
| D3 | `referee_sim_node` `provide_nav` 解耦 | 独立模式回归不变；双仓库模式不再抢 action server |
| D4 | `tools/demo.sh --with-nav [--nav-silent]` | 一键起导航 + 决策；面板显示真实点位 |
| D5 | 文档与 `CHANGELOG` | ARCHITECTURE / USAGE / BELIEF 同步 |

## 5. 运行（目标形态）

```bash
# 独立仿真（默认，与现在一致）
tools/demo.sh

# 双仓库联调（真实导航，headless）
tools/demo.sh --with-nav --nav-silent
```

双仓库模式下 `demo.sh` 负责：确保 `rm27net` 存在 → 起导航容器并 launch
（`slam:=False gui:=false use_rviz:=false`，挂载 `pcd/simulation` 到 `/pcd`）→ 等
`/navigate_to_pose` 就绪 → 起决策容器（同网络，`referee_sim_node -p provide_nav:=false`，
`decision_node --ros-args -p odom_topic:=/odometry -p odom_frame:=odom`）→ 收尾清理。
