# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P2 策略迁移）

### 已锁定决策

- 战略层：纯 C++ `StrategicPolicy` 接口（输入 `WorldState`，输出 `StrategicDecision`），在树 tick 前求值写入 `DecisionContext.strategy`；不是 BT 子树。
- 删除「姿态」（stance）：P2 验收输出为 `nav_goal` / `tactical_mode` / `resource` / `cmd_vel`，不含姿态。
- 弃用 `/set_bool`（`Reloading` / `ifreload`）：P2 不迁移该行为。
- 树结构：任务 / 技能 / 条件分层为多棵小树，避免旧仓库一整棵大树；`tree/` 按层建 `mission/`、`skill/`、`condition/`，相似树可再分子目录、零散的直接放层根。
- 命名：条件节点 / 子树 `If*`；任务子树 `Mission*`；技能子树 `<Verb><Object>`；发 Intent 叶子 `Emit*`；写状态叶子 `Set*`；请求叶子 `Request*`。
- 配置：单一入口 `config/profiles.yaml` + `config/maps/*.yaml` + `config/policies/*.yaml`；core 只定义纯结构，bringup 用 `yaml-cpp` 加载；XML 不写数值。
- 迁移基准：结构借鉴 `navi_minco_bit/bt_manager`，行为以旧 `sentry_DecisionMaking/RMUC.xml` + 参考仓库启用分支为准，先最小可用。
- 模块粒度（P2）：`common` / `nav` / `resource` + `strategic`（C++）+ `intervention`（默认关）。
- P2 暂不做 zones（区域判定），只用命名点与距离。

### 进行中

- 分支 `p2-strategy`（基于 `main` @ `4958c8b`）。
- 文档已同步：`ARCHITECTURE.md` §3.2 / §6 / §7.3 / §7.5 / §11 / §14；`ROADMAP.md` P2 子阶段与状态。
- **P2.0 完成（本地）**：
  - `config/`：单一入口 `profiles.yaml` + `maps/RMUL26.yaml` + `policies/rmuc26.yaml`；core `PolicyConfig` 纯结构；bringup `config_loader` 用 yaml-cpp 加载并校验。
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
- 待补：功能域 `.so` 拆分与 `module.yaml`（provides / consumes 校验）——当前启用/禁用已由 `tree_manifest.yaml` + 启动校验承担，`.so` 分离属编译期整理，优先级低于策略迁移。
- **P2.3a 动作派发与 ack 完成（本地）**：
  - core `ActionDispatcher`：one-shot 由无到有只发一次、polled 按 interval 重发、ack 按 `request_id` 清除、超时 WARN；纯逻辑，宿主单测。
  - `submit_resource_requests` 把 `ResourceRequest` 转成动作；`RosIoNode::take_acks` 把 `DecisionAck` 转 ROS 无关 `ActionAck` 供决策线程消费。
  - `DecisionActuatorSim` 模拟执行端延迟回执，`decision_main` 离线跑通 ack 闭环。
  - 验证：宿主 + 容器 7 包 / 23 测试 0 失败。
- **P2.3b 待办（不实现）**：auto-aim 侧 `DecisionCommand` → 串口字节帧、动作 `code` 取值表、`detect_color` 编码——待与电控 / MCU 确认协议后再做。
- 下一步：P2.4 `SafeSupervisor` 与 `intervention` core 侧注入。

### 历史（P1 / P2 接口）

- P1 已完成（本地范围）：裁判协议解码、`WorldState` 契约对齐、`sentry_decision_msgs` + `DecisionStatePublisher`、core 回放 `ReplaySource`、rosbag 读取、本地仿真。
- 回归：core 回放确定性单测 + 树级 `replay_determinism`。
- 暂缓：真实下位机通信包 io 接线；旧 rosbag 字段一致性对比（暂无样本）。
- 已移除：`sentry_info_3` 解码（疑似临时规则）。
- 已确认：下位机动作分 `kOneShot` / `kPolled`，`kPolled` 带轮询间隔，见 `ARCHITECTURE.md` §4.1。
- P2 上位机接口重构已合入 `main`：`sentry_interfaces` + `decision_node` 真实 IO 闭环。
- 待办：Offline/Radar 上行、`DecisionAck` 回传、下行动作到 MCU 串口帧——待与电控/MCU 确认协议。
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
- 未确定项集中记录在 `docs/ARCHITECTURE.md` 第 16 节。
