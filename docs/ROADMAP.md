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

## P0 骨架（已完成）

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

## P1 信念与回放（已完成，本地范围）

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

子阶段（每步可独立构建 / 测试）：

| 子阶段 | 内容 | 验收 |
| --- | --- | --- |
| P2.0 工程底座（已完成） | `tree/` 骨架（mission / skill / condition）、`tree_manifest.yaml`、builtin 模块注册与启动校验、`PolicyConfig` 与 YAML 加载 | 容器构建 / 19 测试通过；缺失配置、树引用不存在 key 均启动即失败（负例用例覆盖） |
| P2.1 战略层（已完成） | `StrategicPolicy` 接口 + `RuleBasedStrategicPolicy` 规则状态机 + 表驱动单测；接入 `DecisionContext`，输出 `kTacticalMode` 意图 | 给定 `WorldState` → 期望 `TacticalMode` |
| P2.2 nav_policy + nav_executor（已完成） | `mission/nav/*`（撤退 / 补给 / 防守 / 高地 / 进攻 / 巡逻）+ `skill/goto_named_point`；战略模式驱动任务选择；`nav_executor` 回写 `NavState`；接仲裁 | 表驱动 `WorldState → nav_goal`，覆盖旧 `RMUC.xml` 分支 |
| P2.3a 动作派发与 ack（已完成） | `ActionDispatcher`（one-shot / polled / ack 状态机）、资源请求→动作提交、`DecisionActuatorSim` 本地 mock、`RosIoNode::take_acks` | one-shot 只发一次、polled 按间隔重发、ack 清除、超时 WARN 有单测；离线 ack 闭环通过 |
| P2.3b 串口适配（待协议） | auto-aim 侧 `DecisionCommand` → 串口帧、`code` 取值表、`detect_color` | 与电控 / MCU 联调；本阶段不实现 |
| P2.4 安全与干预（core 侧已完成） | `SafetySupervisor` 限幅 / 急停；`InterventionController` 意图注入、世界覆盖、模块开关（ROS action / service 留 P3） | 安全压过 intervention / tactical；急停与 lease 有宿主单测 |
| P2.5 回归（已完成，本地范围） | `NavGoalTracker` 目标边沿 / 取消契约、抢占契约测试、回放确定性、表驱动 golden；差异报告待旧 bag | 抢占 / halt 用例通过；回放确定性；文档同步 |

> P2.0 已完成（本地范围）：分层树骨架、`tree_manifest.yaml` + 启动校验、`config/` 配置外置与 `PolicyConfig`、命名点 / 配置 key 解析。
> P2.1 战略层已完成：`StrategicPolicy` 接口、规则状态机、`apply_strategy` 接入。
> P2.2 nav_policy + nav_executor 已完成：六个任务子树 + 战略模式驱动选择；`nav_executor` 状态回写。
> P2.3a 动作派发与 ack 已完成：`ActionDispatcher` + `DecisionActuatorSim` 离线闭环。
> 功能域 `.so` 拆分（`common` / `nav` / `strategic`）与 `module.yaml` provides / consumes 校验已完成。
> P2.4 core 侧已完成：`SafetySupervisor` 与 `InterventionController` 已接入两个入口。
> P2.5 回归已完成（本地）：`NavGoalTracker` 契约 + 抢占测试 + 回放确定性。
> `resource` 模块已拆分并接入；仍待办：P2.3b 串口字节层（待电控 / MCU）、旧 bag 差异报告。

交付物：

- 行为树目录与 `tree_manifest.yaml`、模块清单 `module.yaml`、插件加载与启动校验。
- `StrategicPolicy` 接口 + 首个实现（纯 C++，非 BT 插件）。
- 分层小树：`tree/mission/`（任务）+ `tree/skill/`（技能）+ `tree/condition/`（条件子树）。
- 命名点与阈值配置外置（`config/`）。
- `nav_executor` 与 `IntentArbiter` 接入，`SafeSupervisor` 兜底。
- `intervention` 模块（意图注入、世界状态注入、模块开关）。

验收：

- 对同一组场景，新旧输出（导航目标、战术模式、资源请求）一致，并有脚本化差异报告；姿态已废弃，不纳入比对。
- 抢占与 `halt` 取消契约有测试覆盖。
- 无旧 rosbag 前，先用表驱动 golden 用例（`WorldState → DecisionOutput`）承载旧行为基准。

## P3 可视化与仿真

**目标**：决策过程可观测、可手动干预、可赛后回放。

子阶段（每步可独立构建 / 测试）：

| 子阶段 | 内容 | 验收 |
| --- | --- | --- |
| P3.0 观测底座（已完成） | `TreeNodeStatus` / `TreeStatus` 消息、`sentry_decision_viz` 的 `TreeStatePublisher`（节点状态 + active path）、Groot2 可选 hook | `/decision/tree_status` 状态与 active path 正确；容器测试通过 |
| P3.1 裁判仿真 + 场景脚本（已完成） | `referee_sim_node`（发 `/sentry/*` + odom、导航 action server、动作回执）+ 场景 YAML 时间轴与断言 | 脚本驱动完整对局（巡逻→进攻→撤退→复活）并通过断言 |
| P3.2 干预 ROS 接口（已完成） | `ManualOverride.action` + `DebugCommand.srv`、`InterventionServer`（线程安全队列、tick 边界应用）、`/decision/intervention` | 接口可注入并看到逐字段胜负；模块开关生效 |
| P3.3 干预回放（已完成） | `ReplayData` 增干预通道、`ReplaySource` / `load_replay_data` 支持 | 含干预的回放逐 tick 确定，干预在原时刻复现 |
| P3.4 网页面板 | rosbridge + roslibjs 静态页：树状态、战场俯视图、WorldState / Intent、按钮组 | 网页实时显示树状态与机器人 / 敌方位置；按钮生效 |
| P3.5 文档与验收 | ARCHITECTURE / ROADMAP / NOTE / USAGE / CHANGELOG 同步；完整对局录制 + 回放 | 三条验收闭环 |

交付物：

- `TreeStatePublisher`（节点状态 + active path）、Groot2 可选连接。
- rosbridge 战场页：树状态、战场俯视图、WorldState / Intent 面板、干预按钮。
- `referee_sim_node` 与场景脚本。
- 干预 action / service 与可回放记录。

验收：

- 网页实时显示树状态与机器人 / 敌方位置。
- 裁判仿真能驱动完整对局。
- 人工干预可记录并在回放中复现。

设计细节见 `docs/ARCHITECTURE.md` §14。

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

- **P0 完成**：仓库与架构文档；core（契约 / 仲裁 / 日志 / 信念 / IO 抽象）；io 的 ROS 适配器；nodes 最小插件；bringup 最小闭环；Docker、Dev Container、CI（含格式检查）。
- **P1 完成（本地范围）**：
  - 已交付：裁判协议位段解码、`WorldState` 契约按 `ros_interfaces` 对齐、`sentry_decision_msgs` 与 `DecisionStatePublisher`、core 确定性回放 `ReplaySource`、rosbag 读取 `load_replay_data`、本地仿真 `sentry_decision_sim`。
  - 回归：core 回放确定性单测 + 树级 `replay_determinism`（同一份回放两次输出逐 tick 一致）。
  - 暂缓：真实下位机通信包 io 接线（包定义待定，本地开发用仿真）；旧 rosbag 字段一致性对比（暂无可用旧 bag，待提供样本）。
- **P2 进行中**：
  - 已完成：上位机接口重构（`sentry_interfaces` + `docs/INTERFACES.md`；决策侧 `RosIoNode`；auto-aim 侧桥；`decision_node` 真实 IO 闭环），已合入 `main`。
  - 已完成（本地）：P2.0 工程底座——`tree/` 分层骨架、`tree_manifest.yaml` + 启动校验、`config/` 配置外置与 `PolicyConfig`、命名点 / 配置 key 解析；容器 7 包 / 19 测试通过，宿主 core 测试通过。
  - 已完成（本地）：P2.1 战略层——`StrategicPolicy` 接口 + `RuleBasedStrategicPolicy` 规则状态机、`DecisionContext::apply_strategy`、两个入口接入、表驱动单测；容器 20 测试通过。
  - 已完成（本地）：P2.2 nav_policy + nav_executor——六个 nav 任务子树、`IfTacticalMode` / `IfEnemyOutpostDead` 条件、命名点驱动、`nav_executor` 状态回写；容器 21 测试通过。
  - 进行中：P2 策略迁移，设计已对齐——战略层为纯 C++ `StrategicPolicy` 接口、任务 / 技能实现为分层小树、配置外置、删除姿态、弃用 `/set_bool`（见 `docs/ARCHITECTURE.md` §3.2 / §6 / §7.3 / §7.5 / §15）。
  - 已完成（本地）：P2.3a 动作派发与 ack——`ActionDispatcher`（one-shot / polled / ack / 超时）、资源请求→动作、`DecisionActuatorSim` 本地 mock、`RosIoNode::take_acks`；容器 23 测试通过。
  - 已完成（本地）：功能域 `.so` 拆分——`common` / `nav` / `strategic` 独立库、`tree_manifest.yaml` 按 `library` 加载、`module.yaml` provides / consumes 启动校验；容器 24 测试通过。
  - 已完成（本地）：P2.4 core 侧安全与干预——`SafetySupervisor`（限幅 / 急停）、`InterventionController`（意图注入 lease、世界覆盖、模块开关），已接入 `decision_node` / `decision_main`；容器 26 测试通过。
  - 已完成（本地）：P2.5 回归——`NavGoalTracker` 目标边沿 / 取消契约（接入两个入口）、抢占契约测试、回放确定性；容器 28 测试通过。
  - 已完成（本地）：`resource` 模块——资源 / 复活独立 `.so`、`tree/resource/root.xml` 与导航任务并行、经字段级仲裁合并；容器 29 测试通过。
  - 待办：P2.3b 串口字节层（待电控 / MCU 协议）；旧 bag 逐 tick 差异报告。
- **P3 进行中**：
  - 设计已对齐（本地）：新增 `sentry_decision_viz`、树状态消息与 `/decision/tree_status`、Groot2 可选、干预 action / service（线程安全队列、tick 边界应用）、`referee_sim_node` 与场景脚本、干预回放、无构建网页面板；见 `docs/ARCHITECTURE.md` §14。
  - 已完成（本地）：P3.0 观测底座——`TreeNodeStatus` / `TreeStatus` 消息、`sentry_decision_viz` 的 `TreeStatePublisher`（`/decision/tree_status`）、Groot2 可选 hook（`--groot2-port`）、`decision_node` 接线；容器 8 包 / 35 测试通过。
  - 已完成（本地）：P3.1 裁判仿真 + 场景脚本——`referee_sim_node`（五条上行 + odom、`NavigateToPose` action server、`DecisionCommand`→`DecisionAck`）与场景 YAML（`set_world` / `expect`）；`scenario/full_match.yaml` 端到端跑通巡逻→进攻→撤退→复活（12/12 断言）；容器 8 包 / 37 测试通过。
  - 已完成（本地）：P3.2 干预 ROS 接口——`ManualOverride.action` / `DebugCommand.srv` / `InterventionEvent.msg`、`InterventionServer`（线程安全队列、tick 边界应用、action 生命周期反馈）、仲裁逐字段 `winners`、`/decision/intervention` 记录；容器 8 包 / 39 测试通过，含 `intervention_smoke`。
  - 已完成（本地）：P3.3 干预回放——`InterventionCommand` 归入 core 并由实时 / 回放共用 `apply_intervention`；`ReplayData` 增干预通道、`ReplaySource::interventions()` 按时刻返回；`load_replay_data` 读取 `/decision/intervention`；回放确定性测试含人工接管。
  - P2 主体已完成（本地范围）。
