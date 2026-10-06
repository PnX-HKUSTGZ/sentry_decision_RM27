# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（v0.4.1 功能修复与初版决策树）

### 已锁定决策

- 复活完整建模（方案 B，规则 5.2.2）：战亡即开始读条，读条完成且已确认才复活，血量为 10% 上限血；
  读条在补给区 / 己方基地 <2000 血时每秒 +4。仿真裁判按死活推导 `info1.can_free_resurrect`，
  不再依赖场景手动置位。
- 协议电平：确认免费复活 / 兑换立即复活（`0x0120` bit 0 / 1）是电平位，决策侧从 OneShot 改为
  Polled，间隔 `action.revive_poll_interval_ms`（默认 200ms）。停止提交即停止置位；MCU 侧需保证
  不复活时 bit0 归零（正式接板时确认，P2.3b）。

### 已完成

- 复活完整修复（规则 5.2.2）：`sim_world` / `referee_sim_node` 读条与 10% 血量复活，复活动作改 kPolled；
  `full_match` 覆盖阵亡→确认→复活。已提交 f3d54f4。
- 初版主决策树（还原上一赛季）：战略层三条规则 + 任务树重排（进攻 > 高地循环 > 后方巡逻 > 守堡垒）；
  新增 PatrolLoop 技能节点（到点停留 nav.patrol_dwell_s 秒）与 tree/skill/patrol_loop.xml；
  游标存 DecisionContext.patrol。移除进攻时间窗。
- 撤退迟滞：低于 retreat_hp 进入撤退后保持到 recovery_hp 才离开补给区；迟滞状态放
  DecisionContext.strategy_memory，StrategicPolicy::decide 增 memory 参数。
- 双仓库「重置」回起点：`referee_sim_node` 在 `provide_nav=false` 下建 `NavigateToPose` 客户端，
  重置时发回起点目标（map 新增 `start`，sim.yaml 新增 `nav_start`）；独立模式仍瞬移。
- 文档：ARCHITECTURE §14.4 / USAGE / CHANGELOG / NOTE 同步。

### 待办

- 初版决策树其余修复项（待补）。

## 历史 sprint（v0.4.0 双仓库联调）

### 已完成

- 移除决策内部仿真暂停：`SetGamePause.srv`、`referee_sim_node` 暂停服务、`MatchStageController` /
  `NavSimulator` 暂停状态、网页面板暂停按钮、`match_smoke` 暂停断言。
- 定位走真实重定位：PCD 落位 `sentry_navgation_RM27/src/pb2025_nav_bringup/pcd/simulation/RMUC2026.pcd`，
  `slam:=False` 下 `relocalization_manager` `LOCALIZING -> LOCALIZED`，`map->base_footprint` ≈ 出生点。
  曾评估的 `slam:=True` 与静态 `map->odom` 方案均放弃（SLAM 以起点为原点，无法与绝对地图对齐）。
- 决策侧方案 A（tf2）：`decision_node` 支持 `--ros-args` 透传；`RosIoNode` 新增 `odom_frame`，
  用 TF `map->odom` 把 odom 位姿转 map 系；`pose_to_map` 纯逻辑 + 单测。
- `referee_sim_node` 新增 `provide_nav`：双仓库时 `false`，不自建导航 action / odom，位姿取
  `/decision/world_state`。
- `tools/demo.sh` 新增 `--with-nav` / `--nav-silent`；`docs/sim_dual_repo.md` 记录接口契约。
- 端到端实测：`tools/demo.sh --with-nav --nav-silent`，比赛开始后决策下发 `(1.39, 6.08)`，
  真实 Nav2 驱动机器人从出生点移动到 `(-9.33, -4.10)`（map 系）。
- 抖动修复：`timeouts.odometry_ms` 200 -> 500（`/odometry` 由点云回调驱动、约 10Hz）；
  实测静止 90s 与运动 25s 均无新增急停。`demo.sh` 无 X11 时自动关导航 RViz、导航容器退出时快速报错。
- 容器 `colcon test`：8 包 / 49 测试 0 失败。

### 待办

- 无（下一步见 `docs/sim_dual_repo.md` 与 `docs/ROADMAP.md`）。

## 当前 sprint（P3 可视化与仿真）

### 已锁定决策

- 分支 `p3-viz-sim`（基于 `p2-strategy` @ `5e39311`），上游 `origin/p3-viz-sim` 已设置。
- 子阶段：P3.0 观测底座 → P3.1 裁判仿真 + 场景脚本 → P3.2 干预 ROS 接口 → P3.3 干预回放 → P3.4 网页面板 → P3.5 文档与验收（详见 `docs/ROADMAP.md`）。
- 包边界：新增 `sentry_decision_viz`（`TreeStatePublisher` + Groot2 桥 + 网页资产 + launch）；ROS 仿真节点 `referee_sim_node` 放 `sentry_decision_sim`（core 仿真库保持 ROS 无关）；`InterventionServer` 放 `sentry_decision_io`；干预 action / service 消息放 `sentry_decision_msgs`。
- 树状态：`TreeStatus` / `TreeNodeStatus` 消息，`Tree::applyVisitor` 展平；active path 取 RUNNING 节点集合（运行中节点的祖先必然也在运行）；`/decision/tree_status`（transient local）。
- Groot2：可选 `--groot2-port`（BT.CPP 4.9 已带 zmq），不作为 CI 验收。
- 干预线程模型：service / action 回调只校验入队，tick 边界 drain 进 `InterventionController`；应用结果发 `/decision/intervention` 供录制回放。
- 网页：vanilla JS + vendored `roslib.min.js`（锁版本、BSD-2），原生 ES modules 分层（bridge / store / panels / battlefield），无 npm 构建；为将来迁移 Vite + TS 留接缝。
- P2.3b 串口字节层仍待 MCU 协议，排除在 P3 外。
- 设计细节同步至 `docs/ARCHITECTURE.md` §14。

### 进行中

- 已完成（v0.3.1，本地）：移除决策内部人工干预（意图注入 / 世界覆盖 / 运行期模块开关），
  保留仿真世界干预与战术层覆盖。实现：core 新增 `TacticalOverride` + `apply_tactical_override`，删除 `intervention.*` /
  `kIntervention` / `/decision/intervention` / `ManualOverride.action` / `InterventionEvent.msg`；io 用 `PanelService`（`list_state` /
  `set_tactical_mode` / `clear_tactical_mode`）替换 `InterventionServer`；场景去掉 `add_intent` / `disable`；面板收敛。
  容器 8 包 47 测试 0 失败。计划见 `docs/ROADMAP.md`「v0.3.1」。
- 文档先行（已完成）：`ARCHITECTURE.md` 按目标态改写（§9 优先级 / §10 战术层覆盖 / §14.3 面板服务 / §14 面板与场景）；
  `USAGE.md` 同步收敛（§4.7 战术层覆盖、移除干预按钮与 `/decision/intervention`）；`BELIEF.md` 去掉决策侧世界覆盖。
- 已锁定（本地）：补给区圆心并入 `sim_home`（删除 `healing`），新增 `sim_base` 表示基地增益点；基地 / 前哨站增益点仅兑换、不回血；
  补给区回血加进入延时 `supply.enter_delay_s`（默认 1s）；仿真世界干预收敛为 `/sentry_sim/set_world` 与新增的
  `/sentry_sim/apply_effect`（步长 `sim.yaml: effects`，面板三行按钮：自身 / 我方 / 敌方）；面板删除「决策覆盖」与「强制撤退」。
  决策内部干预（意图注入 / 世界覆盖 / 运行期模块开关）已立项，见 ROADMAP v0.3.1。
- 已锁定（本地）：地图点支持可选第 4 位区域半径，补给区 / 增益点半径从 `sim.yaml` 移入 `maps/*.yaml`；
  面板按半径把增益区画成青色虚线环（`list_state.points.r`）；标定工具初始空序列，列表可直接编辑
  x/y/yaw/r 并按 r 画环，新增「刷新预览」重绘。标定工具的文件读写（需本地后端）已放弃。
- 已完成（本地）：P3.8 面板布局 + 参数外置——左列 60% 放战场俯视图（上）/ 行为树，右列 40% 控制；
  「场地标定」新标签页打开；俯视图叠加命名点（`list_state` 提供，重置后重读）；
  新增 `config/sim.yaml`（`max_hp` / 导航速度 / 比赛阶段时长 / 补给区 / 增益点），`referee_sim_node` 全部改从 yaml 读，`--sim-config` 可覆盖路径；
  决策侧 `safety` / `timeouts` / `action` 旋钮进 `config/policies/<POLICY>.yaml`；离线 `decision_main` 的 `NavSimulator` 速度也来自 sim.yaml；
  所有可调常量从源码移出（`MatchStageController` 阶段时长、`WorldTimeouts`、`SafetyLimits`、`ActionDispatcher` 超时）。
- 已完成（本地）：P3.7 资源动作接入 + 场地标定——`ResourceRequest` 增 `remote_ammo` / `remote_hp` / `instant_revive`，
  资源树按「立即复活 > 免费复活 > 远程血 > 本地弹 > 远程弹」重写，血量只走脱战远程兑换；
  场地口径确认为 RMUC（战场 28×15m），地图 profile 更名 `RMUC26`，网页面板「场地标定」工具完成标定，
  主面板战场俯视图改用导航仓库 `RMUC2026.pgm` 底图（0.05 m/px、origin [-14.6,-5.86]）；
  仿真增益点默认坐标、场景 start_pose / expect 与相关冒烟同步到新点位。
- 已完成（本地）：P3.6 仿真机制补全 + 鲁棒性审查——补给区免费发弹量（规则 5.3.2，按整分钟累积、进区领取）、
  远程兑换 6 秒延迟（含远程血量 6 秒内战亡作废）、增益点区域默认启用并补 `our_outpost` 坐标；
  修复 `local_ammo_exchange_point()` 漏判 RMUL 补给位（bit 2）、`NavigateToPose` 旧 goal 悬挂、网页面板位姿空值崩溃；
  `test_sim_world` / `test_resource` 扩测，`sim_effects_smoke` 增加补给发弹与远程延迟端到端断言。
- 文档已同步：`ARCHITECTURE.md` §14（可视化与仿真）与 §5 / §10.6 / §12 / §17 引用；`ROADMAP.md` P3 子阶段与状态；`CHANGELOG.md` Unreleased。
- 已完成（本地）：P3.0 观测底座——`TreeNodeStatus` / `TreeStatus` 消息、`sentry_decision_viz` 包与 `TreeStatePublisher`（`/decision/tree_status`）、Groot2 可选 hook（`--groot2-port`）、`decision_node` 接线；容器 8 包 / 35 测试通过，`format.sh --check` 通过。
- 已完成（本地）：P3.1 裁判仿真 + 场景脚本——`referee_sim_node`（发 `/sentry/*` + odom、`NavigateToPose` action server、`DecisionCommand`→`DecisionAck`）、ROS 无关的场景解析与消息级 `SimWorld`、`scenario/full_match.yaml`；容器 8 包 / 37 测试通过，含端到端 `scenario_full_match`。
- 已完成（本地）：P3.2 干预 ROS 接口——`sentry_decision_msgs` 新增 `ManualOverride.action` / `DebugCommand.srv` / `InterventionEvent.msg`；io 新增 `InterventionServer`（action / service 只入队，tick 边界应用）与取值解析（yaml-cpp）；core `IntentArbiter` 新增逐字段 `winners`、`InterventionController` 支持按字段清除；`decision_node` 发布 `/decision/intervention` 并提供 `list_state` JSON；容器 8 包 / 39 测试通过，含 `intervention_smoke`。
- 已完成（本地）：P3.3 干预回放——`InterventionCommand` 归入 core，实时与回放共用 `apply_intervention`；`ReplayData.interventions` + `ReplaySource::interventions()` 按时刻返回；`load_replay_data` 读取 `/decision/intervention`；core / bringup / io 测试覆盖干预通道与含干预的回放确定性。
- 已完成（本地）：裁判协议补全——`EventCode` 覆盖 0x0101 全字段；`SentryInfo2` 增加 `stance` / `stance_enhanced`，新增 `SentryInfo3`（姿态剩余时长）；`UpstreamState.info3`、io 合并与 sim `sentry_info_3` 同步；单测覆盖。
- 已完成（本地）：P3.4 网页面板——`sentry_decision_viz/web` 纯静态页（vendored `roslib.min.js` 1.4.1、原生 ES modules、`web/package.json` 仅声明 `type: module`）；`bridge` / `store` / `format` / `panels/*` / `battlefield` 分层；`viz.launch.py` 起 rosbridge + 静态服务；`web/test/format.test.mjs` node 单测（CI `viz-js-tests`）与 `tools/viz_smoke_test.sh`；`docker/Dockerfile` 加 `ros-jazzy-rosbridge-suite`。

### 待办

- P3.5 收尾（本地）：已完成——场景 `add_intent` / `disable`、`replay_main` 离线回放与 `replay_smoke`；实际实机 bag 复盘待 P4。
- 已完成：参考文档 `refs/26UC`（抽取到 `refs/26UC/_text/`）补齐 `referee_protocol` 的 `event_code` 全字段与 `sentry_info_3`；`detect_color` 为视觉字段、文档未定义，仍待 auto-aim 确认。
- 场景脚本 `add_intent` / `disable` 接入（干预通道已就绪）。
- P4：实车 / 仿真跑通、旧仓库归档、迁移报告。

## 历史（P2 策略迁移，已完成）

### 已锁定决策

- 战略层：纯 C++ `StrategicPolicy` 接口（输入 `WorldState`，输出 `StrategicDecision`），在树 tick 前求值写入 `DecisionContext.strategy`；不是 BT 子树。
- 姿态（stance）：2026 规则 §5.6.4 为有效机制；已恢复接入（`SentryStance` / `IntentField::kStance` / `DecisionOutput.stance`），由战略层按战术模式映射并随仲裁输出。
- 弃用 `/set_bool`（`Reloading` / `ifreload`）：P2 不迁移该行为。
- 树结构：任务 / 技能 / 条件分层为多棵小树，避免旧仓库一整棵大树；`tree/` 按层建 `mission/`、`skill/`、`condition/`，相似树可再分子目录、零散的直接放层根。
- 命名：条件节点 / 子树 `If*`；任务子树 `Mission*`；技能子树 `<Verb><Object>`；发 Intent 叶子 `Emit*`；写状态叶子 `Set*`；请求叶子 `Request*`。
- 配置：单一入口 `config/profiles.yaml` + `config/maps/*.yaml` + `config/policies/*.yaml`；core 只定义纯结构，bringup 用 `yaml-cpp` 加载；XML 不写数值。
- 迁移基准：结构借鉴 `navi_minco_bit/bt_manager`，行为以旧 `sentry_DecisionMaking/RMUC.xml` + 参考仓库启用分支为准，先最小可用。
- 模块粒度（P2）：`common` / `nav` / `resource` + `strategic`（C++）+ `intervention`（默认关）。
- P2 暂不做 zones（区域判定），只用命名点与距离。

### 进行中

- 分支 `p2-strategy`（基于 `main` @ `4958c8b`）。
- 文档已同步：`ARCHITECTURE.md` §3.2 / §6 / §7.3 / §7.5 / §11 / §15；`ROADMAP.md` P2 子阶段与状态。
- **P2.0 完成（本地）**：
  - `config/`：单一入口 `profiles.yaml` + `maps/RMUC26.yaml`（P2 时曾用 `RMUL26.yaml`）+ `policies/rmuc26.yaml`；core `PolicyConfig` 纯结构；bringup `config_loader` 用 yaml-cpp 加载并校验。
  - `tree/`：`root.xml` → `mission/root.xml`（优先级）→ `mission/nav/*` + `skill/goto_named_point.xml`；命名规范 `If*` / `Mission*` / `Emit*` 落地。
  - `tree_manifest.yaml` + `tree_loader`：builtin 模块注册、`*_key` / `*point` 引用的配置启动校验。
  - 节点：`CheckLowHp` → `IfLowHp`（`hp_key`），新增 `EmitNavGoalFromPoint`（命名点）；删除 `demo_tree.xml`。
  - 修复：core 静态库补 `POSITION_INDEPENDENT_CODE`（被 `nodes.so` 链接本来会失败）；子树独立黑板，节点统一从根黑板取 `context`。
  - 验证：容器 7 包构建通过、`colcon test` 19 测试 0 失败；宿主 `host_core_test.sh` 全通过；`format.sh --check` 通过。
- **P2.1 战略层完成（本地）**：
  - core `StrategicPolicy` 接口 + `StrategicDecision{TacticalMode}`；`DecisionContext::apply_strategy` 写策略并提交 `kTacticalMode` 意图。
  - `RuleBasedStrategicPolicy`（优先级：复活 > 撤退 > 补给 > 防守 > 进攻 > 巡逻），阈值可由 `nav.retreat_hp` / `nav.low_ammo` / `strategic.attack_window_*` 覆盖。
  - `decision_main` / `decision_node` 在树 tick 前求值；`DecisionOutput.tactical_mode` 由此产出。
  - 验证：宿主测试 + 容器 7 包 / 20 测试 0 失败。
- **P2.2 nav_policy + nav_executor 完成（本地）**：
  - 任务树：`mission/nav/{retreat,supply,defend,highland,attack_outpost,patrol}.xml`，优先级撤退 > 补给 > 防守 > 高地 > 进攻 > 巡逻。
  - 条件节点：`IfTacticalMode`（读战略模式）、`IfEnemyOutpostDead`；技能 `GotoNamedPoint` 由任务传命名点。
  - `nav_executor` 回写 `NavState`（current_goal / reached / failed）已有，经 `WorldModel` 生效。
  - 验证：`test_mission_nav` 表驱动 6 场景；容器 7 包 / 21 测试 0 失败。
- **功能域 `.so` 拆分完成（本地）**：
  - `sentry_decision_nodes` 拆为 `sentry_decision_common` / `sentry_decision_nav` / `sentry_decision_strategic` 三个独立库，各自 `BT_REGISTER_NODES` 自注册（strategic 为非 BT 插件）。
  - `tree_manifest.yaml` 按 `library` 加载模块；库目录由 `sentry_decision_nodes_DIR` 在配置期回推（`DEFAULT_MODULE_LIB_DIR`）。
  - `tree/modules/*.yaml` 声明 provides / consumes；启动校验「消费字段有提供者」「provides 不重复」，配负例测试。
  - 验证：容器 7 包 / 24 测试 0 失败。
- **P2.3a 动作派发与 ack 完成（本地）**：
  - core `ActionDispatcher`：one-shot 由无到有只发一次、polled 按 interval 重发、ack 按 `request_id` 清除、超时 WARN；纯逻辑，宿主单测。
  - `submit_resource_requests` 把 `ResourceRequest` 转成动作；`RosIoNode::take_acks` 把 `DecisionAck` 转 ROS 无关 `ActionAck` 供决策线程消费。
  - `DecisionActuatorSim` 模拟执行端延迟回执，`decision_main` 离线跑通 ack 闭环。
  - 验证：宿主 + 容器 7 包 / 23 测试 0 失败。
- **P2.3b 待办**：auto-aim 侧 `DecisionCommand` → 串口字节帧、`DecisionAck` 回传、动作 `code` 取值表、`detect_color` 编码——待与电控 / MCU 确认协议后再做。auto-aim 侧 ROS 接口迁移（5 条上行消息 + `Subscribe2Decision`）已完成。
- **P2.4 core 侧完成（本地）**：
  - core `SafetySupervisor`：仲裁后最终限幅 + 数据失效急停（撤销导航目标、速度清零）；在两个入口接入并按状态变化记日志。
  - core `InterventionController`：意图注入（统一 `kIntervention` 来源 / 优先级 + lease）、世界状态覆盖（类型化 `WorldField`）、模块开关；走同一仲裁，不能绕过安全。
  - 验证：`test_safety_supervisor` / `test_intervention`；容器 7 包 / 26 测试 0 失败。
- **P2.5 回归完成（本地）**：
  - core `NavGoalTracker`：把导航目标的「发送 / 取消 / 不变」边沿契约显式化，接入 `decision_node` / `decision_main`。
  - `test_preemption`：进攻 -> 低血抢占撤退 -> 恢复进攻，断言目标改派 / 不重发 / 撤销取消，且每 tick 至多一个 nav_goal。
  - 回归：回放确定性、表驱动 golden、抢占契约；容器 7 包 / 28 测试 0 失败。
- **`resource` 模块完成（本地）**：
  - `sentry_decision_resource` 独立 `.so`：`IfCanFreeResurrect` / `IfLowAmmo` / `IfCoinsAtLeast` + `RequestFreeRevive` / `RequestHpExchange` / `RequestAmmoExchange`。
  - `tree/resource/root.xml` 与 `MissionRoot` 并行求值（`ForceSuccess` 包裹），输出 `kResourceRequest`，经字段级仲裁与导航目标合并。
  - 验证：`test_resource` + 容器 7 包 / 29 测试 0 失败。
- **Copilot review 修复（本地）**：
  - `decision_node` 把配置 `map_frame` 注入 `RosIoNode`，不再固定默认 frame。
  - 任务树新增最高优先级 `MissionRespawn`（死亡不发导航目标），复活交给 `ResourceRoot`；补 HP=0 回归。
  - `config_loader`：数值必须完整解析（拒绝 `50oops`）；map 形式点位强制要求 `x`/`y`。
  - `InterventionController::allows`：运行期模块开关按字段过滤意图，接入两个决策循环。
  - 验证：容器 7 包 / 31 测试 0 失败。
- **Copilot review 第二轮修复（本地）**：
  - 世界覆盖不再提升 `upstream.valid`，避免绕过裁判失效急停。
  - `config_loader` 增加已知数值键的整数 / 非负校验（配负例）。
  - 两个决策循环每 tick 清 `kIntervention` 来源，模块关闭后旧干预意图不再残留。
  - `--plugin` 改为真覆盖：`TreeSetupOptions.load_modules=false`，不再叠加 manifest 模块。
  - 验证：容器 7 包 / 33 测试 0 失败。
- 待办：P2.3b 串口字节层；intervention 的 ROS action / service（P3）；旧 bag 逐 tick 差异报告。
- P2 主体已完成（本地范围）；下一步进入 P3 可视化与仿真。

### 历史（P1 / P2 接口）

- P1 已完成（本地范围）：裁判协议解码、`WorldState` 契约对齐、`sentry_decision_msgs` + `DecisionStatePublisher`、core 回放 `ReplaySource`、rosbag 读取、本地仿真。
- 回归：core 回放确定性单测 + 树级 `replay_determinism`。
- 暂缓：真实下位机通信包 io 接线；旧 rosbag 字段一致性对比（暂无样本）。
- 已移除：`sentry_info_3` 解码（疑似临时规则）。
- 已确认：下位机动作分 `kOneShot` / `kPolled`，`kPolled` 带轮询间隔，见 `ARCHITECTURE.md` §4.1。
- P2 上位机接口重构已合入 `main`：`sentry_interfaces` + `decision_node` 真实 IO 闭环；auto-aim 侧 ROS 接口迁移在 auto-aim 分支 `sentry-decision-interface`。
- 待办：Offline 视觉字段上行、下行动作到 MCU 串口帧与 `DecisionAck` 回传——待与电控/MCU 确认协议。
- 阶段与验收见 `docs/ROADMAP.md`；编码与文档规范见 `docs/CONVENTIONS.md`。

## P0 已完成

- 文档与架构（`docs/ARCHITECTURE.md`、`ROADMAP`、`CONVENTIONS`、`USAGE`）。
- `sentry_decision_core`：数据契约、`IntentArbiter`、分级日志器、IO 抽象、`WorldModel`、`DecisionContext`。
- `sentry_decision_nodes`：行为树插件与节点注释模板。
- `sentry_decision_bringup`：最小闭环、演示树与 launch。
- `sentry_decision_io`：`RosIoNode` 实现四个端口接口。
- 基础设施：Docker 镜像（含 `dev` 用户）、Dev Container、`compile_commands.json`、CI（宿主测试 / Jazzy 构建测试 / io 冒烟 / 格式检查）。
- 验证：宿主与容器测试全部通过；实跑展示 `patrol → retreat` 抢占。

## 注意事项

- 当前无法上实车，一律以本地 PC 容器验证为准。
- 不自动执行 Git 操作；不自动执行系统级操作。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 未确定项集中记录在 `docs/ARCHITECTURE.md` 第 17 节。
