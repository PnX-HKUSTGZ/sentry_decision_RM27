# 开发路线图

> 本文档给出分阶段目标、开发顺序、交付物与验收标准，是排期的单一事实来源。
> 每阶段结束需同步更新本文档与 `docs/CHANGELOG.md`。

## 总体原则

- **先核心后外围**：`core` → `io` 抽象 → 最小可运行闭环 → 再迁移策略树。
- **分阶段可验证**：每个阶段都能独立构建、测试、演示，不依赖后续阶段。
- **测试与文档同步**：每阶段补测试，并更新架构 / 规范 / 变更日志。
- **旧仓库为回归基准**：`sentry_DecisionMaking` 冻结为参考，迁移逐分支对照。

## 阶段总览

| 阶段 | 目标 | 主要交付物 | 验收标准 |
| --- | --- | --- | --- |
| P0 骨架 | 能编译、能测试、能跑最小闭环 | core（契约 / 日志 / 配置）、io 接口抽象、bringup、Docker、CI、宿主 core 单测 | Jazzy 容器内 `colcon build && colcon test` 通过；宿主 core 单测通过；日志双通道生效；最小树可按固定频率 tick |
| P1 信念与回放 | 接管 I/O 并打通回放 | 单节点 io、裁判解码 + 单测、WorldState 融合、DecisionState 发布、replay 适配器 | 旧 rosbag 回放得到一致的 WorldState；同一回放可确定性复现 |
| P2 策略迁移 | 用新架构复现旧决策 | 多子树 + 插件清单、战术 / 技能模块、nav_policy + nav_executor、IntentArbiter 接入、intervention 模块 | 同场景下新旧输出（目标点 / 模式 / 姿态）一致；抢占与 halt 契约测试通过 |
| P3 可视化与仿真 | 可观测、可手动干预 | TreeStatePublisher、Groot2、rosbridge 战场页、referee_simulator、场景脚本 | 网页实时显示树状态与机器人 / 敌方位置；仿真驱动完整对局；干预可记录与回放 |
| P4 切换与冻结 | 新仓库成为默认 | 实车 / 仿真跑通、旧仓库打 tag 归档、迁移报告 | 24.04 实车与容器行为一致；文档齐全 |

## P0 骨架（当前）

**目标**：在无 ROS 的宿主上即可测试核心逻辑；在 Jazzy 容器内可构建、可测试、可跑最小闭环。

交付物：

- `sentry_decision_core`：数据契约、`IntentArbiter`、分级日志器、配置校验。
- `sentry_decision_io`：接口抽象（`RefereeSource`、`OdometrySource`、`NavigationSink`、`ChassisSink`）与 ROS 实现骨架。
- `sentry_decision_bringup`：`main`、launch、最小编排树。
- `docker/`：Jazzy 开发镜像与 Compose。
- CI：格式检查 + 构建 + 测试。
- `tools/host_core_test.sh`：宿主纯逻辑测试。

验收：

- 宿主 `tools/host_core_test.sh` 通过。
- Jazzy 容器内 `colcon build && colcon test` 通过。
- 日志双通道按级别正确分流。
- 最小行为树以固定频率 tick，且状态可观测。

## P1 信念与回放

**目标**：由单一 I/O 节点接管全部外部交互，并把「回放」打通为可用的调试底座。

交付物：

- `sentry_decision_io`：单一 `rclcpp` 节点，订阅 / 发布 / action / service 客户端，回调只做解码。
- 裁判协议位段解码（纯函数 + 单测）。
- 信念层：`WorldState` 融合、时间对齐、有效期与降级。
- 发布 `DecisionState`，rosbag 记录 `/decision/*`。
- `ReplaySource`：从录制数据离线驱动 core。

验收：

- 旧 rosbag 回放得到的 `WorldState` 关键字段与旧实现一致。
- 同一回放两次运行输出完全一致（确定性）。

## P2 策略迁移

**目标**：按 `docs/ARCHITECTURE.md` 用新的分层与插件树复现旧仓库的决策。

交付物：

- 行为树目录与 `tree_manifest.yaml`、插件加载与启动校验。
- 战术 / 战略层（`StrategicPolicy` 接口 + 首个实现）。
- 任务 / 技能模块，先 `nav`，再 `stance`、`resource`、`tactical`。
- `nav_executor` 与 `IntentArbiter` 接入，`SafeSupervisor` 兜底。
- `intervention` 模块（意图注入、世界状态注入、模块开关）。

验收：

- 对同一组场景，新旧输出（导航目标、战术模式、姿态）一致，并有脚本化差异报告。
- 抢占与 `halt` 取消契约有测试覆盖。

## P3 可视化与仿真

**目标**：决策过程可观测、可手动干预、可赛后回放。

交付物：

- `TreeStatePublisher`（节点状态 + active path）。
- Groot2 实时连接。
- rosbridge 战场页：树状态、战场俯视图、WorldState / Intent 面板、干预按钮。
- `referee_simulator` 与场景脚本。

验收：

- 网页实时显示树状态与机器人 / 敌方位置。
- 裁判仿真能驱动完整对局。
- 人工干预可记录并在回放中复现。

## P4 切换与冻结

**目标**：新仓库成为默认实现，旧仓库归档。

交付物：

- 新仓库接管实车与仿真启动。
- 旧仓库打 tag 归档，撰写迁移报告。
- 文档补齐（架构、规范、部署、复盘流程）。

验收：

- 24.04 实车与 Jazzy 容器行为一致。
- 一次完整仿真对局可由回放复现并定位问题。

## 当前状态

- 已完成：仓库与架构文档、`sentry_decision_core` 数据契约与 `IntentArbiter`、宿主单测。
- 进行中：core 分级日志器。
- 下一步：IO 接口抽象与 bringup 最小闭环。
