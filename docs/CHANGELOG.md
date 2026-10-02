# Changelog

> 记录会影响用户、开发者或贡献者体验的重要变更。面向人类阅读，应随版本发布更新。
> 格式参考 [Keep a Changelog](https://keepachangelog.com/)。
> [备忘] 变更类型：Added（新增）、Changed（变更）、Deprecated（弃用）、Removed（移除）、Fixed（修复）、Security（安全）。

> [注意] 本文档是项目历史的单一事实来源，应随项目演进保持更新。
> [注意] 本文档不是所有变更的完整清单；只记录重要变更并保持简洁易读。完整变更见版本控制系统（如 Git）历史。

## [Unreleased]

## [v0.3.0] - 2026-10-02

> P3：可视化与本地仿真——让决策可观测、可干预、可回放，并完成规则对齐与参数外置。

### Added

- **观测与面板**：新增 `sentry_decision_viz`（`TreeStatus` / `TreeNodeStatus`、`/decision/tree_status`、
  可选 Groot2）与 rosbridge 网页面板（行为树 / 战场俯视图 / 世界状态 / 模块与 Intent / 干预）；
  新增场地标定工具 `calibrate.html`。
- **本地仿真**：`referee_sim_node` 发布 `/sentry/*` 五条上行与 odom，提供 `NavigateToPose` action server
  与 `DecisionCommand`→`DecisionAck` 闭环；ROS 无关的 `SimWorld` 与场景脚本（`set_world` / `expect` /
  `add_intent` / `disable`）；`tools/demo.sh` 一键起「面板 + 仿真 + 决策」。
- **比赛与场地**：`MatchStageController`（准备 / 自检 / 倒计时 / 比赛中 / 结算，只可前进、可重置、可暂停）；
  按位姿判定增益点占领；补给区回血（10%/s，4 分钟后 25%，进入延时 1s）、免费发弹量（每分钟 100 发、可累积）、
  远程兑换 6 秒延迟与战亡作废；裁判侧动作前置校验。
- **规则对齐**：`referee_protocol` 按 2026 规则 / 通信协议补齐（`EventCode` 全字段、`SentryInfo3`）；
  恢复姿态 `SentryStance` 并贯通战略层 / 仲裁 / 消息 / 面板；资源请求打通远程发弹 / 远程血量 / 立即复活。
- **干预与回放**：`ManualOverride.action` / `DebugCommand.srv` / `InterventionEvent.msg`；`InterventionServer`
  在 tick 边界应用干预；`IntentArbiter` 逐字段 `winners`；`/decision/intervention` 录制；`replay_main` 离线回放。
- **仿真效果**：新增 `srv/ApplyEffect`（`/sentry_sim/apply_effect`），按 `config/sim.yaml` 的 `effects`
  对真实世界施加具名效果（自身扣血 / 扣弹 / 死亡、双方前哨 / 基地扣血与摧毁），面板三行按钮。
- **增益区**：地图点支持可选第 4 位区域半径（补给 / 基地 / 前哨 / 堡垒），`list_state.points` 带 `r`；
  面板把 `r > 0` 的点画成青色虚线环；标定工具可直接编辑 x/y/yaw/r 并预览。
- **工具与测试**：`scenario_smoke` / `intervention_smoke` / `replay_smoke` / `match_smoke` /
  `sim_effects_smoke` / `referee_guard_smoke` / `viz_smoke` 端到端测试，`web/test/format.test.mjs` 纯逻辑单测。

### Changed

- 可调参数全部外置到 YAML：`config/sim.yaml` 与 `config/policies/<POLICY>.yaml`（安全限幅 / 输入超时 /
  动作超时 / 回血 / 效果），源码不再硬编码默认值，缺失即启动失败。
- 地图切换为 RMUC：`map_profile` `RMUL26`→`RMUC26`，主面板改用 `RMUC2026.pgm` 底图；补给区 / 增益点
  圆心与半径来自地图命名点（`sim_home` / `sim_base` / `sim_our_outpost` / `sim_fort`）。
- 面板收敛仿真干预：保留「仿真世界」并新增「仿真效果」；移除「决策覆盖」与与模式下拉等价的「强制撤退」。
- 场地标定工具初始为空序列，避免把示例点位误当成实际配置。

### Fixed

- 裁判来源就绪判定：`GameInfo` + `SentryInfoOnline` 同时到达才判 `RefereeState` 有效。
- `EventCode::local_ammo_exchange_point()` 补判 RMUL 补给位（bit 2）。
- `NavigateToPose` 收到新目标先 `abort` 旧 goal，避免悬挂。
- 本地兑换发弹量门控「占领增益点」、兑换血量要求脱战，裁判侧同步校验并打印拒绝原因。
- 树状态缓存避免整树 IDLE / 残留上一拍 SUCCESS。
- `-p` 数值参数兼容整数 / 浮点写法；面板「重置」恢复初始位姿并清空导航目标。
## [v0.2.0]

### Added

- 新增 `sentry_interfaces`：与 auto-aim 的接口契约（`GameInfo` / `SentryInfoOnline` / `SentryInfoOffline` / `TeamInfo` / `RadarInfo` / `DecisionAck` / `DecisionCommand` / `DecisionAction`）
- 新增 `docs/INTERFACES.md`：上位机接口契约（串口帧、上下行消息、动作语义、ack、迁移阶段）
- `sentry_decision_core`：新增 `DecisionAction` / `ActionMode` / `DecisionSink`，移除 `ChassisSink::set_flag`
- `sentry_decision_io`：`RosIoNode` 迁移到 `sentry_interfaces`（订阅 5 上行 + `DecisionAck`，发布 `DecisionCommand`），新增 `sentry_bridge` 纯转换与单测；io 冒烟话题同步
- `sentry_decision_bringup`：新增 `decision_node`（真实 IO + 行为树 + 仲裁 + `DecisionStatePublisher` 闭环）与 launch，含启动冒烟测试

## [v0.1.0]

### Added

- 初始化项目文档（README、AGENT、ARCHITECTURE、NOTE、CHANGELOG、.gitignore）
- 完成架构设计初版：分层、抢占与仲裁、人工干预模块
- `sentry_decision_core`：数据契约与字段级 `IntentArbiter`，含宿主单元测试
- 新增 `docs/ROADMAP.md` 开发路线图与 `docs/CONVENTIONS.md` 代码 / 文档规范
- `sentry_decision_core`：分级日志器（完整 / 简短双通道），含宿主单元测试
- `sentry_decision_core`：IO 端口抽象与信念层 `WorldModel`（含超时降级），含宿主与容器测试
- `sentry_decision_core`：新增 `DecisionContext`（世界状态 + 每 tick 意图缓冲）
- `sentry_decision_nodes`：最小行为树插件（`CheckLowHp` / `EmitTacticalMode` / `EmitNavGoal`）与节点注释模板
- `sentry_decision_bringup`：`main` 最小闭环（WorldModel → 行为树 → IntentArbiter）、演示树与 launch
- 新增 `docker/`（Dockerfile、entrypoint、compose）、`.dockerignore` 与 CI
- `sentry_decision_io`：ROS IO 适配器（订阅 / 发布 / action / service），含容器冒烟测试
- 新增 `docs/USAGE.md` 使用说明；README 精简为 Quick Start
- 统一 clang-format 18.1.8 格式，新增 `tools/format.sh` 与 CI 格式检查
- 新增 `.vscode`（IntelliSense 与推荐扩展）与 `.devcontainer` 配置
- Docker 镜像新增与宿主同 UID 的 `dev` 用户；构建产出合并的 `compile_commands.json`
- `sentry_decision_core`：裁判协议位段解码 `referee_protocol`（`event_code` / `sentry_info_1/2` 纯函数 + 宿主单测；`sentry_info_3` 待协议明确）
- `sentry_decision_core`：`WorldState` 契约按参考 `ros_interfaces` 字段对齐（含敌方列表 / 经济、自身热量与电容等），`SelfState` 预留 IMU 四元数
- 新增 `sentry_decision_msgs`：`DecisionState` / `WorldState` / `DecisionOutput` 对外消息
- `sentry_decision_io`：`DecisionStatePublisher` 发布 `/decision/state`、`/decision/world_state`，含 core→msg 纯转换与测试
- `sentry_decision_core`：确定性回放 `ReplaySource` / `ReplayData`（ROS 无关，固定步长与时间对齐），含宿主单测
- `sentry_decision_io`：`load_replay_data` 从 rosbag2 读取旧话题并填充 `ReplayData`，含往返测试
- `sentry_decision_sim`：dummy 裁判系统 `RefereeSimulator` 与伪导航 `NavSimulator`（ROS 无关），含宿主单测
- `sentry_decision_bringup`：`decision_main` 改用本地仿真驱动信念层，新增 `--hp-drop` 参数
- `sentry_decision_bringup`：树级 `replay_determinism` 回归测试（同一份回放两次输出逐 tick 一致）

### Changed

- 行为树源文件迁移到仓库根目录 `tree/`；bringup 构建、安装与测试路径同步，运行路径不变

### Fixed

- `decision_main` 校验 `--rate` 必须落在 `(0, 1000]`，并补充越界回归测试
- 目标变化检测纳入 `yaw`，仅改朝向时也会重发导航目标
- `ReplaySource::latest()` 改用二分查找，避免逐 tick 线性扫描
- rosbag 回放改用录制时刻（`send_timestamp`）而非读取时钟
- `NavSimulator` 失败状态立即生效，到达时对齐目标 `yaw`
