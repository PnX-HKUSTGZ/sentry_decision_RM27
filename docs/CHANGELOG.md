# Changelog

> 记录会影响用户、开发者或贡献者体验的重要变更。面向人类阅读，应随版本发布更新。
> 格式参考 [Keep a Changelog](https://keepachangelog.com/)。
> [备忘] 变更类型：Added（新增）、Changed（变更）、Deprecated（弃用）、Removed（移除）、Fixed（修复）、Security（安全）。

> [注意] 本文档是项目历史的单一事实来源，应随项目演进保持更新。
> [注意] 本文档不是所有变更的完整清单；只记录重要变更并保持简洁易读。完整变更见版本控制系统（如 Git）历史。

## [Unreleased]

> 2027 Task 层：原子 Task + TaskRuntime，逐步替代 TacticalMode / Mission 树。

### Added

- 无头策略冒烟：`strategy_smoke` 场景覆盖进攻前哨、Scan、低血 Heal、战亡 Respawn 的完整策略流程；`low_ammo_reset_smoke` 覆盖低弹兑换与重置恢复；`nav_reset_smoke` 覆盖导航成功后重置再启动。
- 面板「资源与动作」显示当前 `TaskSnapshot`（任务、状态、阶段、参数、已进行时间、指标）与上一次 `TaskResult`。
- 世界状态面板显示两处前哨阵地的控制方（我方完全 / 我方临时 / 敌方控制）。
- 世界状态面板移除未启用的“期望 / 当前姿态”行；2027 Task 层当前不输出姿态。
- 仿真控制新增「己方前哨 / 对方前哨」快捷行：切换完全 / 临时 / 未控制，以及敌方占点 / 离开。
- 仿真裁判 2027 前哨控制：增益点 5 s 占点、击毁后的完全 / 临时控制转换、前哨站 500 血重建，以及前哨站增益点按补给区 50% 速率回血（不领免费发弹量）。
- 仿真前哨站归属：`OutpostControlState.owner_is_us`；只有完全控制换到另一方时才重建 500 血，同方恢复不重建。
- 仿真中央高地增益点：`own_outpost_side` + `enemy_outpost_opposite` 并入己方前哨占点，`enemy_outpost_side` + `own_outpost_opposite` 并入敌方前哨占点，半径 0.5 m。
- 决策层前哨站归属 belief：`OutpostOwner`、`WorldState.upstream.our_outpost_owner/enemy_outpost_owner`；HP 上升视为重建并反转归属，1 s 后用 full 控制方校正。
- 独立 `ResourceController` 辅助栏：自己的状态机 / 配置 / 快照，不占主任务栏、不产生 `DecisionEpoch`，主栏有 action 时让位。
- 面板增加 Resource 辅助栏显示；Resource 配置新增低弹阈值、批量、保留金币、驻留与重试冷却。
- 前哨站血量趋势：`OutpostTrend`（1 s 差分 + 最近受击），用于己方半场安全判定。
- `UtilityTaskPolicy`：合法 TaskCall 生成 + 控制/基地/生存/转场/切换成本打分；`strategy.use_utility` 切换，并提供 `profiles_rmuc27_utility.yaml` A/B。
- `TaskResult.call`：保留完整 TaskCall，供 utility 做同点连续性/迟滞。
- 端到端 `utility_smoke`：敌方高地射击位选择 Outpost，打掉后回己方半场。
- 端到端 `utility_defense_smoke`：己方前哨掉到临时控制时回防。
- 端到端 `utility_recover_smoke` / `utility_respawn_recover_smoke`：低血/低弹、复活后自动 Heal 回基地。
- 手动 420s headless soak 场景 `full_match_soak.yaml`（不加入默认 ctest）。
- `Navigate` 新目标 500 ms 内忽略上一个目标的 `nav.failed`，避免 utility/rule 反复选同一点并立即失败。
- `Outpost` 要求目标归属敌方；归属我方 / 未知时失败，`kOwnTemporary` 继续攻击。
- 世界状态面板显示前哨站归属。
- `core/task`：`Task` / `TaskCall` / `TaskPlan` / `TaskSnapshot` / `TaskResult` / `TaskState` 纯逻辑契约。
- `core/task_runtime`：`TaskRegistry`、Task 生命周期、超时、抢占、结果与 `DecisionEpoch`。
- `core/task_policy` / `core/task_runner`：手写 Task 选择 + Policy/Runtime 组合根。
- `task_node`：TaskRunner + RosIoNode + SafetySupervisor 的 2027 主程序；`decision_node.launch.py` 与 `tools/demo.sh` 默认已切到 `task_node`。
- 2027 原子 Task：`Navigate`、`Scan`、`Outpost`、`Respawn`、`Heal`、`Resource`，含宿主单测。
- `points_2027.hpp`：14 个 2027 命名点常量（含 3 个 `highland_area_*` 范围点）。
- `WorldState`：`OutpostControl`、`supply_claimed_minutes`。
- 2027 `0x0101` 事件解码：`EventCode` 按 `own_color` 映射 R/B → own/enemy；`GameInfo.event_code_2` 上行字段。

### Changed

- 所有 Task 不重试；裁判兑换类任务只设 5 s 检测窗口。
- `Heal`：基地补给区按每分钟 100 发累积确认；前哨站只回血。
- `Resource`：辅助栏状态型买弹，5 s 等 `info1.ammo_exchanged_local` 到账；支持 `enemy_outpost_buff`，前哨站增益点要求完全控制且 `hp > 0`。
- 主策略不再选择主栏 `Resource`；低弹买弹交给独立辅助栏。
- 仿真默认/场景数值对齐 2027：基地 10000、前哨站 1000、初始弹 300。
- `TaskRunner`：按 `DecisionEpoch` + 1 s 周期门控策略调用；启动 Task 前执行 `can_start`；`strategy.use_utility` 选择 rule / utility。
- `Respawn`：待复活期间发送 `kFreeResurrect` 确认，不再只等待血量变化。
- `TaskState`：变化即发，并补 10 Hz 心跳。

### Fixed

- 重置后 `nav.reached` / `nav.failed` 残留，下一次 `Navigate` 会误判“导航已结束但不在到达半径内”并反复失败；现在取消目标时清空旧结果。
- 低弹且已在基地时策略反复选择 `Heal`（满血立即成功），任务态反复回到 `idle` 且弹药不增加；现在买弹由独立 Resource 辅助栏处理，主栏不再因此空转。
- `RosIoNode::status` 只在发送导航目标后置 `nav.valid`，导致首个 `Navigate` 永远无法启动；现在以 action server 就绪为可用条件，并在查询时刷新心跳。
- 仿真补给区 / 前哨站回血会把 `self_hp=0` 的机器人“救活”，导致 `Respawn` 被取消；现在战亡期间不执行回血。
- `sentry_bridge` 未把 `event_code` 中已解码的前哨控制状态写入 `UpstreamState`，导致 Task 层始终看到 `kNotOwned`。
- `match_smoke` 仍按 2026 起点坐标校验，切换 RMUC27 后失败。
- `Heal` 离开补给区后 5 s 发弹确认窗口未重置。
- `Resource` 动作在 dispatcher 暂停时被清理，导致 5 s 后误失败。
- 策略恢复当前任务时旧的 pending 计划未撤销。
- 跨局 `HistoryView` 未清空，可能导致新局误选 `Resource`。
- 面板急停状态硬编码为 false。
- `Outpost` / `Resource` / `Respawn` 在裁判数据失效时仍使用旧值。
- `Scan` 是无限任务，rule 策略此前会一直返回当前 `Scan`，导致新出现的前哨目标无法触发进攻；现在 `Scan` 允许每拍重新评估。
- 阵亡机器人仍被算作激活增益点，导致尸体占点/阻止敌方夺回；现在 `self_hp<=0` 时占用为空，己方临时控制会按 5 s 缺席转为敌方完全控制并重建。
- `set_world enemy_outpost_hp/our_outpost_hp = 0` 直接改血量时没有走 `destroy_outpost`，导致「临时控制 + 击毁」不转完全控制/不重建；现在置 0 也走击毁结算，并按当前控制状态推断击毁方（我方完全控制视为敌方击毁，其余视为我方击毁）。
- utility 低血时未优先回基地、低弹时不主动回基地；现在低血/低弹会生成恢复候选，低血时只允许 Heal，Heal 开始后持续到满血，不再被回防/占点抢断。
- 前哨站被击毁后仿真仍上报前哨站增益点、仍按前哨站 buff 回血/兑换；现在 `hp=0` 时 buff 失效。
- `Heal` 选点此前 rule 策略总是回基地，忽略了己方完全控制且存活的前哨站增益点；现在按 `highland_area_center` / `highland_area_enemy` / `highland_area_our` 区域判断：敌方前哨增益点优先，其次己方前哨增益点，区域外回基地；低弹/需领基地免费发弹时强制回基地。
- utility 的 `Heal` 之前只要未满血就会生成前哨站回血候选，轻伤（如 300/400）也会打断占点；现在回血候选要求 `self_hp <= nav.retreat_hp`（默认 100），已开始的 Heal 仍持续到满血。

### Removed

- `ActionRequest.retry_interval` / `max_attempts`、`ActionReport.attempts`、`ActionDispatcher` 重发状态机、resource BT 的 `retry_ms` / `max_attempts` 端口与 `action.*attempts` 配置。
- 旧 Task 架构：`TacticalMode`、Intent/Arbiter、Mission 树、`sentry_decision_nodes`、`decision_node` / `decision_main` / `replay_main`、面板战术层覆盖。
- 旧行为树可视化：`TreeStatus` / `TreeNodeStatus`、`TreeStatePublisher`、Groot2 桥、网页行为树面板与 tree smoke 话题。
- 旧资源协议字段：`ResourceRequest`、`DecisionOutput.resource_*`、`DecisionState.conflict_count` / `warnings`。

## [v0.5.2] - 2026-10-31

> 动作策略下沉：重试间隔 / 次数 / 生效判据不再由 dispatcher 按 `kind` 反推，
> 改由行为树节点随动作一起传入；兑换数量与最小单位也在工厂层统一处理。

### Added

- `ActionRequest` / `ResourceIntent`：动作语义（`target` / `retry_interval` / `max_attempts` /
  `verify` / `level`）随意图经仲裁带到 dispatcher。
- `core/action_factory`：集中处理累计目标值、0x020D 判据、最小兑换单位（10 / 100）与 1000 发上限。
- `Request*` 节点新增 `retry_ms` / `max_attempts` 端口，`RequestAmmoExchange` 新增 `amount`，
  远程兑换节点新增 `times`；省略时回退到 config。

### Changed

- `ActionDispatcher` 变成通用重试引擎，删除 `ActionDispatcherConfig` 与按 `kind` 的
  `delay_for` / `max_attempts_for` / `compute_target` / `action_effective`。
- 配置加载新增规则校验：`resource.exchange_ammo_step` 必须是 10 的倍数；
  `resource.remote_*_times` 必须为 1（协议要求每次只能 +1）。

## [v0.5.1] - 2026-10-31

> 通信简化：取消执行回执，改为「下发累计目标值 → 用裁判上行校验 → 未生效重试」。
> 依据 2027 通信协议 V1.0.0，0x0120 的兑换字段本身就是累计值，重复下发幂等。

### Added

- `ActionDispatcher` 生效校验：`action_effective()` 用 0x020D 累计值 / 血量判断；新增配置项
  `action.verify_delay_ms` / `action.max_attempts` / `action.revive_verify_delay_ms` / `action.revive_max_attempts`。
- 面板 `list_state` 新增 `last_action_report`（生效 / 放弃、尝试次数）。

### Changed

- 下行 `DecisionAction.value` 改为协议累计目标值 / 电平位；决策层增量在派发器内折算，
  同一目标重复下发幂等。
- 串口 `DecisionCommand` 由 12 B 缩为 9 B（删除 `mode` / `interval_ms`）。
- 仿真执行端按累计值结算，重复下发不再重复扣金币。

### Removed

- 删除 `DecisionAck` ROS 消息、`/sentry/decision_ack` 话题、串口 `type=0x03` 回执帧与 `Gimbal` 回执队列。
- 删除本地兑换血量动作（`kHpExchange` / `resource.hp`）：0x0120 无对应字段。

## [v0.5.0] - 2026-10-11

> v0.5.0：重构与下位机的通信链路——auto-aim 独占串口并把协议升级为带版本与 CRC 的统一帧 v1，
> 决策不再接触串口，双方只通过 ROS（`sentry_interfaces`）交互；同时统一信念层命名为 `upstream`
> 并移除未启用字段。

### Added

- auto-aim 侧串口协议 v1：统一帧 `[0xAA][version][type][length][payload][crc16]`（CRC-16/MCRF4XX），
  `RefereeUplink`（29 B，裁判上行）/ `DecisionCommand`（12 B，决策下行）/ `DecisionAck`（6 B，回执）；
  与旧 `0x5A` / `0xA5` / `0xA6` 帧按首字节共存在同一串口。协议见 auto-aim `docs/serial_protocol.md`。
- auto-aim 侧 `Subscribe2Decision`：订阅 `/sentry/decision_command`，按 v1 组帧下发，并把 MCU 回执
  发布到 `/sentry/decision_ack`；`Gimbal` 新增 `send_decision_command` / `pop_decision_ack` 与 v1 帧解析。

### Changed

- 通信链路归属：由「决策直接对接 MCU」改为「auto-aim 独占串口 + 决策 ↔ auto-aim 走 ROS」；
  `sentry_interfaces` 成为双方唯一契约（见 `docs/INTERFACES.md`）。
- 信念层一致性改名：`RefereeState` → `UpstreamState`，`RefereeSource` → `UpstreamSource`；
  `WorldState.referee` / `WorldTimeouts.referee` / `ReplayData.referee` → `upstream`；
  配置键 `timeouts.referee_ms` → `timeouts.upstream_ms`、`safety.require_referee` → `safety.require_upstream`；
  消息字段 `referee_valid` → `upstream_valid`。
- auto-aim `Publish2DecisionMaking` 改为发布 4 条 `sentry_interfaces` 上行；补齐
  `game_status` / `game_time_remaining` / `coin_remaining` / `sentry_info_1/2` 字段。
- 文档体系整理：新增 `docs/SIMULATION.md`（仿真、场景、面板与标定），`docs/USAGE.md` 收敛为命令速查；
  `INTERFACES.md` / `BELIEF.md` / `ARCHITECTURE.md` / `ROADMAP.md` / `NOTE.md` 同步。

### Removed

- 移除未启用的雷达 / 变形链路：auto-aim 雷达桥、`RadarInfo` / `EnemyRobotStatus` 的空字段、
  决策侧 `EnemyState.enemies` 与相关信念字段。
- 移除旧 `DecisionToGimbal`（`0xA7`，`ifreload`）帧与对应发送路径，由 v1 `DecisionCommand` 取代。

### Fixed

- 修复 auto-aim 决策接口分支的编译阻断与构建耦合：恢复 `gimbal.hpp` 缺失代码、消除 Ceres 重复查找；
  启用 ROS 时 `sentry_interfaces` 改为硬依赖，避免导航桥被静默跳过。
- CI 适配：clang-format 与 io 冒烟测试同步 `upstream` 改名与雷达移除。

## [v0.4.1] - 2026-10-05

> v0.4.1：功能修复与初版决策树——复活链路按规则 5.2.2 落地，双仓库联调补齐位姿 / 重置 / 面板，
> 并还原上一赛季主策略与巡逻技能。

### Added

- 初版主决策树（还原上一赛季）：`RuleBasedStrategicPolicy` 三条规则——敌前哨存活→进攻敌方前哨；
  敌前哨毁且我前哨存活→中央高地 3 点循环；双方前哨皆毁→剩余大于 `strategic.fort_after_remaining_s`
  后方巡逻，否则回堡垒防守。任务优先级在 `mission/root.xml` 重排。
- 巡逻技能：`PatrolLoop` 节点 + `tree/skill/patrol_loop.xml`——按命名点列表循环，到点后停留
  `nav.patrol_dwell_s` 秒再切下一点；游标显式存于 `DecisionContext.patrol`（按 `loop_id` 区分）。
- 地图新增高地 3 个巡逻点 `highland_a/b/c`。
- 裁判仿真实现复活机制（规则 5.2.2）：战亡后按公式开始复活读条（`10 + round((420-剩余)/10) + 20*累计立即复活次数`），
  每秒 +1、补给区或基地血量 <2000 时 +4；待复活期间导出 `info1.can_free_resurrect`；
  确认免费复活后读条完成即以 10% 上限血量复活；兑换立即复活累计次数并回满血。
- 双仓库联调：`RosIoNode` 新增 `odom_frame`，非空时用 TF `map->odom` 把 odom 系位姿转成 map 系；
  抽出纯逻辑变换 `pose_to_map` + 单测。
- `decision_node` 支持 `--ros-args` 透传，可在启动时覆盖 `RosIoNode` 的 ROS 参数。
- `referee_sim_node` 新增 `provide_nav`（默认 `true`）；双仓库时置 `false`，不自建 `/navigate_to_pose`、
  不发 odom，位姿改取 `/decision/world_state`。
- `tools/demo.sh` 新增 `--with-nav` / `--nav-silent`：起真实导航容器（`rm27net` + PCD）并联调。
- 新增 `docs/sim_dual_repo.md`（双仓库联调设计与接口契约）。

### Changed

- 移除进攻时间窗：改为敌前哨存活即进攻，不再由 `strategic.attack_window_*` 限制。
- `full_match.yaml` 场景改为覆盖「进攻→高地→后方巡逻→守堡垒→撤退→复活」全链路。
- 双仓库「重置」改为下发回起点导航目标：仿真在 `provide_nav=false` 下无法瞬移真实机器人，
  重置时用 `NavigateToPose` 客户端把机器人开回 spawn（`config/sim.yaml` 的 `nav_start` /
  map 的 `start`）；独立模式仍直接瞬移。
- 确认免费复活 / 兑换立即复活改为 `MODE_POLLED`（协议 `0x0120` bit 0 / 1 是电平位），
  重发间隔由 `action.revive_poll_interval_ms` 配置。
- `timeouts.odometry_ms` 由 200ms 调到 500ms：双仓库的 `/odometry` 由点云回调驱动（约 10Hz 且带抖动），
  200ms 会偶发判失效触发急停抖动，进而导致面板位姿闪烁。

### Fixed

- 撤退迟滞：低于 `nav.retreat_hp` 进入撤退后，保持到血量恢复到 `nav.recovery_hp` 才离开补给区
  （设为上限血量即回满才走），不再刚高于进入阈值就切回主任务。迟滞状态显式存于
  `DecisionContext.strategy_memory`，策略接口 `StrategicPolicy::decide(world, memory)` 相应调整。
- 仿真链路中机器人阵亡后不再「只停在 `respawn` 模式」：裁判侧从未按死活导出
  `info1.can_free_resurrect`，且 `kFreeResurrect` 只回执不结算，导致决策不发起确认复活、
  血量也不会恢复。现由仿真裁判统一按规则推导复活状态并结算。
- 网页面板：世界状态键值对齐；右侧拆成「世界状态 / 资源与动作 / 战术层覆盖」三个面板（标题加粗、
  面板间留距）；右栏收窄并保证窄屏时地图占满整行。

### Removed

- 移除仿真暂停：`SetGamePause.srv`、`referee_sim_node` 的暂停服务、`MatchStageController` /
  `NavSimulator` 的暂停状态、网页面板暂停按钮，以及 `match_smoke` 的暂停断言。暂停会引入
  与时间基准 / 恢复相关的额外状态，sim2real 风险高于收益。

## [v0.3.1] - 2026-10-03

> v0.3.1：收敛调试入口——移除决策内部人工干预，只保留仿真世界与战术层覆盖，降低架构复杂度与 sim2real 风险。

### Added

- core 新增 `TacticalOverride`（`set` / `clear` / `mode`）与 `apply_tactical_override`：在战略层求值之后改写
  `context.strategy.mode` 并同步 `kTacticalMode` 意图；网页面板提供设置 / 清除与 lease。

### Changed

- `DebugCommand.srv` 收敛为 `list_state` / `set_tactical_mode` / `clear_tactical_mode`；`list_state` 精简为
  命名点 / 资源请求 / 最近动作 / 动作回执 / 急停 / 战术层覆盖。
- 仲裁优先级带收敛为 `safety > recovery > tactical > default`，字段所有权去掉 intervention 覆盖者。
- 网页面板控制面板移除 Intent / 模块 / 世界覆盖，保留仿真世界 / 仿真效果与战术层覆盖。

### Removed

- 决策内部人工干预：意图注入、世界状态覆盖、运行期模块开关，以及 `InterventionController`、
  `Priority::kIntervention` / `SourceId::kIntervention`。
- `ManualOverride.action`、`InterventionEvent.msg`、`/decision/intervention` 录制与回放通道、`InterventionServer`。
- 场景脚本 `add_intent` / `disable` 与 `scenario/intervention.yaml`；`intervention_smoke` 端到端测试。

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
