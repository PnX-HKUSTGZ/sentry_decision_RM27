# 开发路线图

> 本文档给出分阶段目标、开发顺序、交付物与验收标准，是排期的单一事实来源。
> 已完成阶段只保留摘要与验收结论，详细变更见 [CHANGELOG.md](CHANGELOG.md)；当前进度见 [NOTE.md](NOTE.md)。

## 总体原则

- **先核心后外围**：`core` → `io` 抽象 → 最小可运行闭环 → 再迁移策略树。
- **分阶段可验证**：每个阶段都能独立构建、测试、演示，不依赖后续阶段。
- **测试与文档同步**：每阶段补测试，并更新架构 / 规范 / 变更日志。
- **旧仓库为回归基准**：`sentry_DecisionMaking` 冻结为参考，迁移逐分支对照。

## 阶段总览

| 阶段 | 目标 | 状态 |
| --- | --- | --- |
| P0 骨架 | 能编译、能测试、能跑最小闭环 | ✅ v0.1.0 |
| P1 信念与回放 | 接管 I/O 并打通确定性回放 | ✅ v0.2.0 |
| P2 策略迁移 | 用新架构复现旧决策 | ✅ 主体完成（P2.3b 串口待 MCU） |
| P3 可视化与仿真 | 可观测、可仿真、可回放 | ✅ v0.3.0 / v0.3.1 |
| v0.4.x 双仓库联调与决策树 | 真实导航接入、初版主决策树 | ✅ v0.4.0 / v0.4.1 |
| v0.5.0 通信链路重构 | auto-aim 独占串口，决策 ↔ auto-aim 走 ROS | ✅ |
| v0.5.1 通信简化 | 取消回执，下发累计目标值并用裁判上行校验重试 | ✅ |
| v0.5.2 动作策略下沉 | 重试策略由行为树 `ActionRequest` 传入，dispatcher 通用化 | ✅ |
| P4 切换与冻结 | 实车 / 仿真跑通，旧仓库归档 | ⏳ 进行中 |

## 已完成阶段（摘要）

### P0 骨架（v0.1.0）

`core`（契约 / 仲裁 / 日志 / 信念 / IO 抽象）、`io` ROS 适配器、`nodes` 最小插件、`bringup` 最小闭环、
Docker / Dev Container / CI（含格式检查）。验收：宿主与容器 `colcon build && colcon test` 通过，
最小行为树按固定频率 tick 且状态可观测。

### P1 信念与回放（v0.2.0）

裁判协议位段解码、`WorldState` 融合与有效期、`sentry_decision_msgs` + `DecisionStatePublisher`、
core 确定性回放 `ReplaySource` 与 rosbag 读取 `load_replay_data`、本地仿真 `sentry_decision_sim`。
验收：core 回放确定性单测 + 树级 `replay_determinism`（同一份回放两次逐 tick 一致）。

### P2 策略迁移

| 子阶段 | 内容 | 状态 |
| --- | --- | --- |
| P2.0 工程底座 | `tree/` 分层骨架、`tree_manifest.yaml` + 启动校验、配置外置与 `PolicyConfig` | ✅ |
| P2.1 战略层 | `StrategicPolicy` 接口 + `RuleBasedStrategicPolicy` 状态机、`apply_strategy` 接入 | ✅ |
| P2.2 nav_policy + nav_executor | 六个 `mission/nav/*` 子树、命名点驱动、`nav_executor` 回写 `NavState` | ✅ |
| P2.3a 动作派发与生效校验 | `ActionDispatcher`（累计目标值 / 观测校验 / 重试 / 放弃）、`DecisionActuatorSim` 离线闭环 | ✅ |
| P2.3b 串口适配 | auto-aim 侧 ROS 迁移 + 串口 v1 收发已完成；MCU 固件待实现 | ⛔ MCU |
| P2.4 安全与干预 | `SafetySupervisor` 限幅 / 急停；干预已收敛为仿真世界 + 战术层覆盖（v0.3.1） | ✅ |
| P2.5 回归 | `NavGoalTracker` 目标边沿 / 取消契约、抢占契约、回放确定性 | ✅ |

验收基准：无旧 bag 前先用表驱动 golden（`WorldState → DecisionOutput`）承载旧行为；抢占 / `halt` 有测试覆盖。

### P3 可视化与仿真

| 子阶段 | 内容 | 状态 |
| --- | --- | --- |
| P3.0 观测底座 | `TreeNodeStatus` / `TreeStatus`、`TreeStatePublisher`、Groot2 可选 hook | ✅ |
| P3.1 裁判仿真 + 场景脚本 | `referee_sim_node` + 场景 YAML 时间轴与断言 | ✅ |
| P3.2 干预 ROS 接口 | 后于 v0.3.1 收敛为只读面板服务（仿真世界 + 战术层覆盖） | ✅ |
| P3.3 干预回放 | 后于 v0.3.1 移除干预通道，`replay_main` 只回放世界输入 | ✅ |
| P3.4 网页面板 | rosbridge + roslibjs 静态页、战场俯视图、场地标定工具 | ✅ |
| P3.5 文档与验收 | 三条验收闭环、完整对局录制 + 回放 | ✅ |

设计细节见 [ARCHITECTURE.md](ARCHITECTURE.md) §14；操作见 [SIMULATION.md](SIMULATION.md)。

### v0.3.1 移除决策内部人工干预

调试只保留「仿真世界」与「战术层」两类入口，删除意图注入 / 世界覆盖 / 运行期模块开关、
`ManualOverride.action` / `InterventionEvent.msg` / `/decision/intervention`；core 新增 `TacticalOverride`。
验收：决策节点不再接受任何直接改写意图 / 世界视图的接口；安全层优先级不变；`colcon test` 全绿。

### v0.4.x 双仓库联调与初版决策树

- v0.4.0：`decision_node` 支持 `--ros-args` 透传；`RosIoNode` 用 TF `map->odom` 把 odom 位姿转 map 系；
  `referee_sim_node` 新增 `provide_nav`；`tools/demo.sh --with-nav` 接入真实导航容器；
  端到端实测决策目标驱动真实 Nav2 移动。设计与契约见 [sim_dual_repo.md](sim_dual_repo.md)。
- v0.4.1：初版主决策树（战略层三条规则 + 巡逻技能 `PatrolLoop`）、撤退迟滞、双仓库「重置」回起点、
  裁判仿真复活读条（规则 5.2.2）与 10% 上限血量复活。
- 移除仿真暂停；`timeouts.odometry_ms` 调整到 500ms 以适配约 10Hz 的 `/odometry`。

### v0.5.0 通信链路重构

目标：**auto-aim 独占串口**，决策不再接触串口；决策 ↔ auto-aim 全部走 ROS（`sentry_interfaces`）。

- 决策侧：`RosIoNode` 订阅 4 条 `/sentry/*` 上行，发布 `/sentry/decision_command`；
  `ActionDispatcher` 承载动作派发状态机。契约见 [INTERFACES.md](INTERFACES.md)。
- auto-aim 侧（分支 `sentry-decision-interface`）：`Publish2DecisionMaking` 迁到 `sentry_interfaces`，
  新增 `Subscribe2Decision`；`io::Gimbal` 实现协议 v1（`RefereeUplink` / `DecisionCommand`）；
  移除雷达桥与旧 `DecisionToGimbal`。协议见 auto-aim `docs/serial_protocol.md`。
- 信念层一致性改名：`RefereeState` → `UpstreamState`，端口 / 成员 / 配置键 / 消息字段统一为 `upstream`；
  移除未启用的雷达 / 变形字段。

### v0.5.1 通信简化

- 下行改为裁判协议要求的累计目标值 / 电平位（0x0120），重复下发幂等。
- 删除串口 `type=0x03` 与 ROS `DecisionAck`；决策侧用 0x020D 累计值 / 血量判断生效，
  未生效按配置重试，超过次数上限后放弃。
- 串口 `DecisionCommand` 12 B → 9 B；删除本地兑换血量（0x0120 无对应字段）。
- 详见 [CHANGELOG.md](CHANGELOG.md) v0.5.1 与 [INTERFACES.md](INTERFACES.md) §6。

### v0.5.2 动作策略下沉

- 新增 `ActionRequest` / `ResourceIntent` / `action_factory`；dispatcher 只做通用重试。
- 行为树节点通过 `retry_ms` / `max_attempts` / `amount` / `times` 端口传入策略与数量。
- 最小兑换单位（10 / 100）与 1000 发上限在工厂 / 配置加载层校验。

## P4 切换与冻结

**目标**：新仓库成为默认实现，旧仓库归档。

交付物：

- 新仓库接管实车与仿真启动，与导航仓库联合跑通。
- MCU 侧实现串口 v1（裁判上行 / 决策下行），实机闭环。
- `SentryInfoOffline` 视觉锁定字段与 `TeamInfo.allies` 接入决策。
- 旧仓库打 tag 归档，撰写迁移报告；文档补齐（架构、规范、部署、复盘流程）。

验收：

- 24.04 实车与 Jazzy 容器行为一致。
- 一次完整仿真对局可由回放复现并定位问题。

## 当前状态与待办

- **已完成**：P0 / P1 / P2（除 MCU 串口）/ P3、v0.3.1、v0.4.x、v0.5.0、v0.5.1。
- **待办**：
  - MCU 实现串口 v1 帧，实机验证决策 ↔ auto-aim ↔ MCU 全链路。
  - `SentryInfoOffline` 视觉锁定字段（`is_get` / `armor_pos` / `armor_num`）与
    `TeamInfo.allies[4]` 接入 `EnemyState` / `WorldState.allies`。
  - 旧 rosbag 逐 tick 差异报告（等可用样本）。
  - 实车与导航仓库联合跑通、旧仓库归档与迁移报告。
- 开放问题集中记录在 [ARCHITECTURE.md](ARCHITECTURE.md) §17。
