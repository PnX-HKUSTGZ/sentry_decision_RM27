# Changelog

> 记录会影响用户、开发者或贡献者体验的重要变更。面向人类阅读，应随版本发布更新。
> 格式参考 [Keep a Changelog](https://keepachangelog.com/)。
> [备忘] 变更类型：Added（新增）、Changed（变更）、Deprecated（弃用）、Removed（移除）、Fixed（修复）、Security（安全）。

> [注意] 本文档是项目历史的单一事实来源，应随项目演进保持更新。
> [注意] 本文档不是所有变更的完整清单；只记录重要变更并保持简洁易读。完整变更见版本控制系统（如 Git）历史。

## [Unreleased]

### Added

- 新增 `docs/ARCHITECTURE.md` §14「可视化与仿真」，以及 `docs/ROADMAP.md` P3 子阶段规划：树状态 `TreeStatus`、Groot2 可选接入、干预 action / service、裁判仿真与场景脚本、干预回放、rosbridge 网页面板。
- `sentry_decision_msgs`：新增 `TreeNodeStatus` / `TreeStatus` 消息
- 新增 `sentry_decision_viz` 包：`TreeStatePublisher`（发布 `/decision/tree_status`）与可选 Groot2 桥
- `decision_node`：新增 `--groot2-port` 参数（默认关闭）
- `sentry_decision_sim`：新增 `referee_sim_node`（发布 `/sentry/*` 与 odom、提供 `NavigateToPose` action server、`DecisionCommand`→`DecisionAck`）与 ROS 无关的场景解析 / 消息级仿真世界
- 新增场景脚本 `scenario/full_match.yaml` 与端到端测试 `tools/scenario_smoke_test.sh`（巡逻→进攻→撤退→复活）
- `sentry_decision_msgs`：新增 `ManualOverride.action` / `DebugCommand.srv` / `InterventionEvent.msg`
- `sentry_decision_core`：`IntentArbiter` 结果新增逐字段 `winners`；`InterventionController` 支持按字段清除与只读视图
- `sentry_decision_io`：新增 `InterventionServer`（干预 action / service 服务端与线程安全命令队列）及取值解析
- `decision_node`：接入干预服务端，发布 `/decision/intervention`，`list_state` 返回 JSON 快照
- 新增 `tools/intervention_smoke_test.sh` 干预冒烟测试
- `sentry_decision_core`：`InterventionCommand` 归入 core，实时与回放共用 `apply_intervention`；`ReplayData` 增干预通道，`ReplaySource::interventions()` 按时刻返回
- `sentry_decision_io`：`load_replay_data` 读取 `/decision/intervention` 并重建干预命令
- `sentry_decision_viz`：新增 rosbridge 网页面板（`web/` 静态页、vendored `roslib.min.js`、`viz.launch.py`），含树状态 / 战场 / 世界状态 / 模块与干预面板及干预按钮
- 新增 `web/test/format.test.mjs`（node 纯逻辑单测，CI `viz-js-tests`）与 `tools/viz_smoke_test.sh`
- `docker/Dockerfile`：新增 `ros-jazzy-rosbridge-suite`
- `sentry_decision_core`：`referee_protocol` 按 2026 规则 / 通信协议补齐——`EventCode` 覆盖场地事件全字段，`SentryInfo2` 增加姿态，新增 `SentryInfo3`（姿态剩余时长）；`RefereeState` 增 `info3`
- `sentry_decision_io`：`sentry_bridge` 解码 `sentry_info_3`
- `sentry_decision_sim`：`SimWorld` 支持 `sentry_info_3` 场景字段
- 场景脚本支持 `add_intent` / `disable`（经 `/decision/debug` 注入干预），新增 `scenario/intervention.yaml` 与 `scenario_intervention` 测试
- `sentry_decision_bringup`：新增 `replay_main` 离线回放入口；`load_replay_data` 支持新格式 `/sentry/*` 上行
- 新增 `tools/replay_smoke_test.sh`（录制一局场景 bag 后离线重放并校验干预复现）
- 恢复并接入姿态（stance，2026 规则 §5.6.4）：`SentryStance`、`IntentField::kStance`、`DecisionOutput.stance`、`decode_stance`；战略层按战术模式映射（进攻→进攻姿态、防守→防御姿态、其余→移动姿态），并贯通仲裁、消息与网页面板
- `referee_sim_node` 新增 `--hold`（场景时间轴跑完后不退出）与 `scenario/demo.yaml` 面板演示世界
- `sentry_decision_msgs`：新增 `SetGameStage.srv`；`referee_sim_node` 提供 `/sentry_sim/set_game_stage` 与 ROS 无关的 `MatchStageController`（准备 / 15s自检 / 5s倒计时 / 比赛中：只可前进、按秒自动推进、可重置），网页面板新增比赛状态栏与阶段按钮
- 新增 `tools/match_smoke_test.sh` 比赛阶段冒烟测试
- `TacticalMode::kIdle`：战略层只在 `game_status=4`（比赛中）执行任务，准备 / 自检 / 倒计时 / 结算输出 idle；`MissionPatrol` 以 `IfTacticalMode` 门控，待机时不下发任务目标
- 准备阶段改为 3 分钟倒计时；网页面板状态栏用 `MM:SS` 白色显示
- 网页面板新增世界覆盖表单（自身 / 基地 / 前哨血量、金币、发弹量、剩余时间，走 `/decision/debug set_world`）与「暂停 / 继续」；后者新增 `SetGamePause.srv` 与 `/sentry_sim/set_game_pause`，暂停时冻结计时与机器人运动；场景新增可选 `start_pose`
- `list_state` 增加 `resource` / `last_action` / `last_ack`，网页面板显示资源请求、最近下发的决策动作与执行回执；`decision_node` 下发动作时记 `[action]` ACT 日志
- `sentry_decision_msgs`：新增 `SetWorld.srv`；`referee_sim_node` 提供 `/sentry_sim/set_world`，可直接修改仿真世界的真实数值（面板「仿真世界」行）
- `sentry_decision_sim`：`SimWorld` 新增兑换（`exchange_ammo` / `exchange_hp`，规则 5.3.1 金币结算）与补给区回血（`supply_heal`，近似规则 5.2.1）纯逻辑及 `test_sim_world`；`DecisionActuatorSim` 可在回执时把兑换结算进世界
- `referee_sim_node` 新增补给区回血（可配置圆心 / 半径 / 回血比例 / `max_hp`），并新增 `tools/sim_effects_smoke_test.sh` 端到端校验兑换与回血
- `list_state` 增加 `raw_world`（覆盖前的真实世界），网页面板在决策覆盖生效时显示「原始 血/弹/金」

### Fixed

- 行为树面板整树 IDLE：BT.CPP 4.10 会在完成的 tick 末尾 `resetStatus()`，改为 `TreeStatusRecorder` 订阅状态变化并缓存可见状态，已完成节点保留 SUCCESS / FAILURE
- `list_state` 始终列出内置四模块（`nav` / `strategic` / `resource` / `recovery`）的生效开关，未显式设置时按默认启用展示
- 网页面板日志区加高；战场俯视图补充示意场地底图（边界 / 中线 / 中圈 / 半场标注）
- 战场俯视图 `mode` 文字移至右上角，避免与左上角半场标注重叠
- 人工接管战术模式（面板「切换模式」/「强制撤退」）现在会在本拍覆盖 `context.strategy.mode`，真正切换任务树分支
- `scenario/demo.yaml` 补充 `enemy_base_hp`，避免面板把敌方基地血量显示成 0（并非读数错误）
- 树状态缓存改为每拍 `clear()`：未执行的分支不再残留上一拍的 SUCCESS（此前待机阶段会误显示攻击分支 SUCCESS）
- 网页面板人工干预改走 `/decision/debug set_intent` service：vendored roslib 1.4.1 的 `ActionClient` 是 ROS 1 actionlib 命名，无法对接 ROS 2 `ManualOverride` action，导致「切换模式 / 前往点位」等按钮无效果
- 战场半场标注改为沿 x 轴左右划分（己方 x<0 / 敌方 x>0）；此前按 y 轴上下标注，导致 `home`(-5,3) 看起来在敌方半场
- 「重置」现在同时恢复机器人初始位姿并清除导航目标（此前只重置世界与阶段，位置不动）
- 未进入「比赛中」时网页面板的人工意图按钮自动禁用，避免准备阶段误发干预
- 战场半场用底色区分（己方 x<0 冷色 / 敌方 x>0 暖色）并加竖向中线，消除此前“上下分”的误读
- 面板 `lease` 默认改为 0（持续到「清空干预」/「重置」），并修正 `lease()` 把 0 误当 5 的问题；有限 lease 按系统时间计算，暂停期间仍会到期
- 面板 `lease` 从「模式」行移到独立「持续」行并加说明
- 「兑换发弹 / 血量」经 `resource_request` 意图转成一次性 `DecisionCommand`，不改变任务树；现在可在面板「资源请求 / 最近动作 / 动作回执」与 `[action]` 日志中确认是否尝试 / 成功
- 裁判仿真的 `DecisionActuatorSim` 此前只回 `DecisionAck` 而不结算：面板「兑换发弹」显示已确认但仿真世界发弹量不变；现在按金币扣减并增加发弹量 / 血量，金币不足回 `accepted=false`
- 裁判仿真此前完全没有补给区回血逻辑；现在机器人处于补给区且比赛中时按上限血量比例回血
- 面板「世界」控件只覆盖决策视图、不改仿真世界，容易被误读为「改了世界却没生效」；现在拆成「仿真世界」（真实生效）与「决策覆盖」（仅视图），并显示覆盖前的原始值

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
