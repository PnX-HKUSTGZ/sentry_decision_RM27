# 项目架构

> 本文档是系统的技术设计文档，也是架构的单一事实来源，面向开发者与 AI 协作者。
> 细节与杂项信息放在其他文档中并在此引用；随系统演进持续更新。

## 1. 概述

本仓库是 RM27 哨兵机器人的决策模块，负责把裁判系统、感知、导航等外部信息，转化为执行层的指令。

决策采用「信念 → 意图 → 指令」的流水线，其中「意图」进一步分为战术 / 战略、任务、技能三层。核心目标：

- **可扩展、模块化、插件式**：新增或开关一个功能模块，不影响其他模块。
- **可独立测试**：每一层都能脱离 ROS 单独测试。
- **可回放**：一场比赛结束后，只凭回放数据即可完整复现并分析决策过程。
- **安全可控**：安全 / 降级是一等公民，人为干预走受控入口，不能绕过安全。

## 2. 设计原则

- **分层解耦**：信念、意图、指令单向依赖，下层绝不反写上层。
- **依赖单向**：`core` 不依赖 ROS；行为树节点不直接调用 ROS，只读写数据契约或调用抽象接口。
- **模块化 / 插件式**：功能模块编译为独立共享库，通过清单加载，可单独开关。
- **可测试**：核心逻辑与 BT 节点是纯逻辑，可用构造好的数据直接单测。
- **回放优先**：所有决策输入与人为干预都可录制、可确定性重放。
- **单一事实来源**：优先级、字段所有权、配置集中定义，避免两套规则。
- **配置即数据**：地图、点位、区域、阈值、模块开关全部外置并做启动校验。
- **观测优先**：模式切换、意图胜负、分支抢占等决策过程必须可观测。

## 3. 总体分层

```mermaid
flowchart TD
  IOIN[IO 层: 订阅 / 发布 / action] --> BELIEF[信念层: WorldState 融合与有效期]
  BELIEF --> STRAT[战术 / 战略层: StrategicPolicy]
  STRAT --> MISSION[任务层: 目标选择与任务分解]
  MISSION --> SKILL[技能层: 行为树 / 原子行为]
  SKILL -->|Intent| ARB[指令层: IntentArbiter 字段级仲裁]
  BELIEF --> SUP[SafetySupervisor 安全监督]
  SUP -->|最高优先级 Intent / veto| ARB
  MAN[人工干预模块 intervention] -->|Intent / 世界状态覆盖 / 模块开关| ARB
  ARB --> OUT[DecisionOutput]
  OUT --> IOOUT[IO 层: 下发 Nav2 / cmd_vel / 服务]
  IOOUT --> BELIEF
```

| 层 | 职责 | 输出 | 实现载体 |
| --- | --- | --- | --- |
| 信念 Belief | 解码、对齐、融合、判定有效期 | `WorldState` | core + IO 适配器 |
| 战术 / 战略 Strategic | 选择姿态（进攻 / 防守 / 兑换 / 复活） | `Posture` / `Objective` | 状态机、效用打分，可换实现 |
| 任务 Mission | 目标选择与任务分解 | `Mission` | 行为树子树 |
| 技能 Skill | 可复用的原子行为 | `Intent` | 行为树子树 / 叶子节点 |
| 指令 Command | 仲裁、限幅、互锁、超时降级 | `DecisionOutput` | `IntentArbiter` + `SafetySupervisor` |

### 3.1 信念层

信念层是决策唯一信任的世界状态。IO 层只做「解码 + 写入原始值」，融合、时间对齐、有效性判定在信念层完成。
每个输入字段都带时间戳与有效期，超时即判定为失效，并触发决策降级（见第 8 节）。

### 3.2 意图层

意图层分三层，接口逐层收窄，每层只产出一层数据：

- **战术 / 战略层**：只回答「现在是什么战术模式（`TacticalMode`）+ 目标（`Objective`）」。
  定义为 `StrategicPolicy` 接口（输入 `WorldState`，输出 `StrategicDecision`），是**纯 C++ 组件**，
  在行为树 tick 之前求值，结果写入 `DecisionContext.strategy` 供任务层读取。
  当前用状态机实现；将来替换为效用打分 / 学习模型只是换一个实现，不影响下层。
- **任务层**：读 `context.strategy` 与 `WorldState`，选择任务并拆解步骤（打前哨、守家、回血、撤退）。
  实现为 `tree/mission/` 下的多棵小树，只做条件判断与 `SubTree` 组合，不直接产生 `Intent`、不写死坐标。
- **技能层**：可复用的原子行为（去某点、巡逻、兑换、等待恢复），把任务转成 `Intent`。
  实现为 `tree/skill/` 下的多棵小树，只发 `Intent`、实现 `halt()`，不关心任务为何被选中。

任务树与技能树通过 `SubTree` 端口的**参数（命名点 / 配置 key）**与 **NodeStatus** 交互；
每棵小树都有独立 `BehaviorTree ID`，可脱离整棵树单独加载测试。分层规则见 §7.5。

### 3.3 指令层

指令层把「树内策略意图 + 树外来源（安全、人工）」在字段级仲裁成唯一、可执行的 `DecisionOutput`。
它不产生新的策略，只做裁决、限幅与安全兜底。详见第 9 节。

## 4. 数据契约

数据契约是模块之间的唯一接口，替代旧仓库散布在 XML 端口上的字符串状态。

```cpp
// 信念：决策看到的世界
struct WorldState {
  RefereeState referee;   // 比赛阶段、基地 / 前哨血量、经济、姿态、强化时间
  SelfState    self;      // 血量、弹量、电容、脱战、当前姿态
  NavState     nav;       // 位姿、当前目标、到达 / 失败、是否在隧道
  EnemyState   enemy;     // 目标锁定、位置、可见性
  std::vector<AllyRobot> allies;
  TimePoint    stamp;     // 快照时间
  // 每个输入字段都应带时间戳 / 有效期，供信念层判定失效
};
```

裁判消息中「未解码」的原始整数（`event_code`、`sentry_info_1/2/3`）不直接进决策，而是先经
`core/referee_protocol.hpp` 的纯函数拆成具名位段（`EventCode`、`SentryInfo1/2/3`），再写入 `RefereeState`。
位段依据 2026 比赛规则手册 V2.2.0 与通信协议 V2.0.0；姿态（`info2.stance` / `info3` 剩余时长）为规则
5.6.4 的有效机制，决策当前不使用，仅做完整解码与观测。
位段定义集中在这一处，协议变更只改这里，并配套 `test/test_referee_protocol.cpp` 单测；io 适配器只负责
把消息字段喂给解码函数，不内联位运算。

### 4.1 下位机通信（暂缓实现）

裁判系统与下位机传感器信息由下位机（MCU）统一采集，决策侧不直接接触裁判串口或字节流。
通信包（上行 / 下行帧）的定义位置尚未确定（不在本仓库与 navigation 仓库），确认前不在 `core` 落地
`UplinkFrame` / `DownlinkFrame`，本仓库只维护「决策视图」的数据契约。`WorldState` 字段与参考消息
`ros_interfaces` 的映射：

| 参考消息 | 对应子状态 | 备注 |
| --- | --- | --- |
| `GameInfo` | `RefereeState` | 阶段 / 时间 / 金币 / 场地事件 / 手动点 / 建筑血量 |
| `TeamInformation` | `RefereeState` + `WorldState.allies` | 己方建筑血量、队友状态 |
| `RadarInfo` | `EnemyState.enemies` + `RefereeState` | 敌方列表、敌方经济、前哨感知 |
| `SentryInfoOnline` | `RefereeState`（自身）+ `SentryInfo1/2` | 血量 / 弹量 / 热量 / 姿态 / 兑换 |
| `SentryInfoOffline` | `EnemyState`（锁定）+ `RefereeState` | 目标锁定、升降、变形、电容、隧道对齐 |

下行指令的语义（待通信包确定后落地）：

- 每个动作在配置时**必须显式选择** `kOneShot`（一次性，如兑换一次发弹量）或 `kPolled`（轮询，如确认复活）；
- `kPolled` 动作带**轮询间隔**，按间隔重发而非每帧重发；`kOneShot` 边沿触发，发送成功后消费，不重复执行。

IMU 姿态（四元数）由下位机传感器提供，决策当前暂不使用，作为 `SelfState.imu` 保留。

```cpp
// 意图：请求，不代表最终生效
enum class IntentField { NavGoal, ChassisVel, ResourceRequest, TacticalMode };

struct Intent {
  IntentField field;
  SourceId    source;    // strategic / mission / skill / intervention / supervisor
  Priority    priority;  // 引用集中定义的优先级表
  TimePoint   stamp;
  Duration    lease{};   // 有效期，过期即失效
  IntentValue value;     // variant
};
```

```cpp
// 指令：仲裁后的唯一输出
struct DecisionOutput {
  std::optional<Point2D> nav_goal;
  std::optional<Twist>   safe_cmd_vel;
  ResourceRequest        resource;
  TimePoint              stamp;
  // 各字段的 owner 与默认值见第 9 节
};
```

## 5. 模块与包划分

初期只落地四个核心包，其余按需再拆，避免过早为包边界付出样板成本。

| 包 | 职责 | 依赖 |
| --- | --- | --- |
| `sentry_decision_core` | 数据契约、裁判协议解码、信念融合、仲裁、几何、日志、配置。**不依赖 ROS** | 标准库 |
| `sentry_decision_msgs` | 对外消息：`DecisionState` / `WorldState` / `DecisionOutput` | std_msgs、geometry_msgs |
| `sentry_interfaces` | 与 auto-aim 的裁判上行 / 决策下行接口消息 | std_msgs、geometry_msgs |
| `sentry_decision_sim` | 本地仿真：dummy 裁判系统、伪导航（ROS 无关）+ ROS 仿真节点 `referee_sim_node` | core、（节点）rclcpp |
| `sentry_decision_io` | 全部 ROS 交互：订阅、发布、action / service 客户端；实现 core 中定义的 IO 接口（real / sim / replay）；干预服务端 `InterventionServer` | core、msgs |
| `sentry_decision_nodes` | 行为树插件模块（含 `intervention`），编译为共享库 | core、io 抽象接口 |
| `sentry_decision_bringup` | `main`、launch、参数 YAML、tree XML、插件清单 | nodes、io、core、viz |
| `sentry_decision_viz` | 可视化：`TreeStatePublisher`、Groot2 桥、网页面板静态资产与 launch | core、msgs、behaviortree_cpp |

`sentry_decision_test`（测试与回放工具）按需再拆。`sentry_decision_msgs` 后续补充干预 / 调试的 action 与 service（见 §14.3）。

### 5.1 包与层的关系

层是逻辑划分，包是构建 / 部署边界，二者是多对多关系：

| 包 | 覆盖的层 |
| --- | --- |
| `sentry_decision_core` | 信念层（融合）+ 指令层（仲裁 / 安全 / 限幅）+ 跨层基础（类型 / 几何 / 日志 / 配置） |
| `sentry_decision_io` | 执行层（传输 / 下发）+ 信念层（原始解码） |
| `sentry_decision_nodes` | 意图层（战略 / 任务 / 技能）+ `intervention` |
| `sentry_decision_bringup` | 无（组合根） |
| `sentry_decision_viz` | 可观测性（跨层只读：树状态、世界快照、意图） |

`SafetySupervisor` 属于指令层，放在 `core` 且不可作为插件关闭；`nodes` 内部以模块（共享库）为真正边界。

```mermaid
flowchart TD
  BRINGUP[bringup] --> NODES[nodes]
  BRINGUP --> IO[io]
  BRINGUP --> CORE[core 不依赖 ROS]
  NODES --> CORE
  NODES -->|仅调用抽象接口| IO
  IO --> CORE
  VIZ[viz 后续] --> CORE
  SIM[sim 后续] --> IO
```

关键约束：行为树节点不得直接创建 ROS 节点或订阅。所有 I/O 集中在 `io` 的单一节点（建议 `rclcpp_components`），
订阅回调只做「解码 + 更新原始值」，BT tick 线程读取不可变快照。

组合根 `bringup` 的 `decision_node` 把 `RosIoNode`、行为树、`IntentArbiter` 与 `DecisionStatePublisher`
放在同一进程/执行器内闭环：IO 回调与 tick 分属不同回调组，tick 每次读取不可变快照。

## 6. 插件机制

使用 BehaviorTree.CPP v4 的共享库插件机制，而不是手写注册清单：

- 每个功能域模块编译为独立 `.so`，内部用 `BT_REGISTER_NODES(factory)` 自注册节点。
- 组合根用 `factory.registerFromPlugin(lib)` 按清单加载；`StrategicPolicy` 等非 BT 逻辑以普通库提供。
- 模块清单 `module.yaml` 声明：`enabled`、`library`、`params`、`nodes`、**提供的输出字段**、**消费的信念字段**。
- 树清单 `tree/tree_manifest.yaml` 声明：入口 XML、各模块开关与覆盖参数。
- 启动时校验（失败即启动报错）：
  1. `enabled` 模块的库可加载；
  2. 会被加载的 XML 引用的节点 / 子树均已注册、文件存在；
  3. 每个消费字段都有生产者；配置里的 key 均存在。
- 新增 / 删除 / 开关模块 = 增删一个库 + 改一行配置，不碰核心代码。

这样才真正实现「独立开关模块而不影响其他模块」。已拆分的模块：`common`（条件）、`nav`（导航技能）、
`resource`（资源 / 复活）、`strategic`（C++ 策略，非 BT 插件）；`intervention` 的 ROS 动作留 P3。
模块库由 `tree_manifest.yaml` 的 `library` 指定、经 `registerFromPlugin` 加载，
`module.yaml` 的 provides / consumes 在启动时校验。

## 7. 行为树规范

### 7.1 控制节点选择

| 标签 | 记忆行为 | 是否可抢占 | 用途 |
| --- | --- | --- | --- |
| `Fallback` | 记住当前孩子，不重查前序 | 否 | 一次性回退 / 恢复 |
| `ReactiveFallback` | 每 tick 从第一个孩子重扫，命中 RUNNING 时 halt 其余 | **是** | 优先级仲裁 |
| `Sequence` | 记住进度，失败重置 | 否（前序条件不重查） | 有副作用的步骤串 |
| `SequenceWithMemory` | 记住进度，失败不重置 | 否 | 可重试的任务步骤 |
| `ReactiveSequence` | 每 tick 从第一个孩子重扫 | **是** | 守卫条件 + 动作 |

### 7.2 节点注释模板

每个 BT 节点必须在头文件类声明上方包含以下模板，缺一即 CI 失败。
**不使用 `===` / `---` 分隔线**：Markdown 预览会把分隔线后的一行误判为标题（Setext heading）。

```cpp
// Node:         CheckEnemyOutpostHealth
// Category:     Condition (synchronous, no side effects)
// Purpose:      判断敌方前哨站血量是否低于阈值，用于前哨进攻分支。
// Inputs:       threshold: int (端口)
//               health:    int (端口, 通常来自 {referee.enemy_outpost_health})
// Outputs:      -
// Blackboard:   read: referee.enemy_outpost_health  write: (none)
// Threading:    tick 在 BT 单线程调用；无阻塞、无 ROS 调用。
// Side Effects: none
// See:          tree/nav/attack.xml -> Priority1AutoOutpostAttack
```

同类节点实现放在同一个 `.cpp` 时，用一行 `// [节点名]` 作为分隔，便于定位（同样避免 Markdown 分隔线）。

### 7.3 目录与命名

行为树按**层**分目录，再按**功能域**分子目录；相似的树可归类到子文件夹，零散的直接放在层目录下，不强制分组：

```text
tree/
├── tree_manifest.yaml     # 入口 XML 与模块、启用开关，启动时校验
├── root.xml               # 只做组合，不写业务逻辑
├── mission/               # 任务层：选任务、拆步骤
│   ├── root.xml           # 唯一写优先级的地方
│   ├── nav/{supply,retreat,outpost,fort,highland,patrol}.xml
│   └── resource/{revive,exchange}.xml
├── skill/                 # 技能层：可复用原子行为，发 Intent
│   ├── goto_named_point.xml
│   ├── supply_and_recover.xml
│   └── patrol_loop.xml
└── condition/             # 需要成子树的条件；简单条件用 If* 叶子节点即可
```

**命名规范**（一眼区分引用的节点 / 子树属于哪一层）：

| 种类 | 前缀 / 形式 | 示例 |
| --- | --- | --- |
| 条件节点 / 条件子树 | `If` | `IfLowHp`、`IfSupplyNeeded` |
| 任务子树 | `Mission` | `MissionSupply`、`MissionRetreat` |
| 技能子树 | `<Verb><Object>` | `GotoNamedPoint`、`SupplyAndRecover` |
| 发 Intent 的叶子节点 | `Emit` | `EmitNavGoal`、`EmitResourceRequest` |
| 写 context 状态的叶子节点 | `Set` | `SetTacticalMode` |
| 发起外部请求的叶子节点 | `Request` | `RequestExchange` |

- 注册名用 `PascalCase`，端口与黑板 key 用 `snake_case`。
- 每个 XML 顶部注释写明「进入条件 / 退出条件 / 抢占关系」。

### 7.4 禁止事项

- 禁止无超时的 `RetryUntilSuccessful(-1)` 与用 `Wait` 轮询业务条件。
- 禁止在 BT 节点里直接 `std::cout` 或直接创建 ROS 对象。
- 禁止保留被注释掉的策略分支（会污染日志与可视化）。
- 禁止新增全局可变状态；所有决策状态显式放进数据契约。

### 7.5 分层与解耦规则

- `root.xml` 只组合，不写业务；
- 优先级只在 `mission/root.xml` 一处定义；
- 任务树只允许出现**条件节点 / 子树**与 `SubTree`，不得直接放发 Intent 的动作叶子，也不得写死坐标 / 阈值；
- 技能树只发 Intent、只读自身端口与配置，不读优先级、不判断「为什么」；
- 条件节点只读 `DecisionContext` / 配置并返回 `NodeStatus`，不得写 Intent、不得有副作用；
- 跨任务复用的逻辑必须下沉到 `skill/` 或 `condition/`；
- XML 不写数值：坐标用命名点（`point="home"`），阈值用配置 key（`hp_key="nav.retreat_hp"`）。

## 8. 抢占

### 8.1 定义

抢占指：一个正在执行的行为 A，被此刻条件成立的更高优先级行为 B 打断，B 接管。它回答「何时停 A、开始 B」。
在 BT.CPP 中，抢占由控制节点是否每 tick 重新扫描高优先级兄弟节点决定；不再 tick 某个 RUNNING 孩子时，父节点对其调用 `halt()`。

### 8.2 可中断契约

所有异步节点必须实现 `halt()`：取消未完成的 action / 服务，清除本次行为写入的意图。否则会出现「树以为停了、机器人还在动」的幽灵行为。
副作用的发起必须是边沿触发（只在 `IDLE → RUNNING` 时发起），不能每 tick 重发。

### 8.3 示例

```text
ReactiveFallback  全局优先级
├── ReactiveSequence  紧急撤退      guard: HP < 30
│   └── SubTree 撤退到基地
├── ReactiveSequence  补充弹药      guard: 弹量 < 阈值
│   └── SubTree 基地购买
└── SubTree 巡逻                   默认
```

血量跌到阈值以下时，下一 tick 紧急撤退分支返回 RUNNING，ReactiveFallback halt 掉正在 RUNNING 的购买分支，
其 `halt()` 取消 Nav2 goal 并发起撤退目标。

## 9. 仲裁

### 9.1 位置

`IntentArbiter` 位于指令层，在「意图产生」与「IO 下发」之间，拦截的是「意图 → 指令」这一步转换。
它是 `core` 中的纯逻辑组件，不依赖 ROS，可脱离行为树单测。

策略树只能表达同一棵树内、互斥分支的优先级；仲裁器额外解决：跨模块 / 树外来源、字段级冲突、
默认值、有效期（lease）、冲突可观测与可测试。

### 9.2 优先级带

优先级集中定义，单一事实来源：

```text
safety(急停 / 看门狗) > intervention(人工干预 / 调试注入) > recovery > tactical > default
```

人工干预走 `intervention` 带，因此硬安全永远压得住人。仿真中可另开仅 `competition_mode=false` 时允许的
`debug_override` 带，用于强制测试。

### 9.3 字段所有权与 lease

| 输出字段 | owner | 允许覆盖者 |
| --- | --- | --- |
| `nav_goal` | 导航决策模块 | safety、intervention |
| `safe_cmd_vel` | recovery（仅接管时） | safety（急停） |
| `resource` | 资源模块 | intervention |

- 非 owner 提交该字段记 `WARN`；
- 同优先级多来源冲突按显式 tie-break 解决并告警；
- lease 过期后字段回落默认值（例如「无目标」），避免机器人固执执行过时目标。

### 9.4 tick 时序

```mermaid
sequenceDiagram
  participant IO as IO
  participant B as 信念层
  participant T as 策略树
  participant A as IntentArbiter
  IO->>B: 刷新 WorldState 快照
  B->>T: 分发 WorldState
  T->>A: 提交 Intent（每 tick 请求缓冲）
  B->>A: SafetySupervisor 高优先级 Intent
  A->>A: 逐字段裁决（优先级 + owner + lease）
  A->>IO: DecisionOutput
  IO->>B: 反馈（到达 / 失败 / 当前点）
```

BT tick 频率默认 20 Hz（可配置），单线程固定频率执行。

### 9.5 与抢占的关系

先树内抢占，再跨来源仲裁：树内用 `ReactiveFallback` / `ReactiveSequence` 打断同策略的低优先级分支；
被 halt 的分支本 tick 不再提交 Intent，其请求自然消失；随后仲裁器把树内意图与树外来源合并成唯一指令。
`SafetySupervisor` 使用保留的最高优先级带，并在仲裁后做最终 clamp / 急停，保证安全无法被绕过。

## 10. 人工干预模块（intervention）

人工干预封装为一个**独立模块**，与策略、安全并列，走同一条流水线，可单独开关；
初期作为 `nodes` 下的独立插件库，接口稳定后如需可提升为独立包。

### 10.1 三个注入点

| 注入点 | 改什么 | 走哪条路 | 用途 |
| --- | --- | --- | --- |
| 世界状态注入 | `WorldState` 字段（血量、弹量、阶段、敌方位置） | IO 的 Sim / Override 源 | 触发策略分支 |
| 意图注入 | 直接加一条 `Intent` | `IntentArbiter` 的 intervention 来源 | 手动接管 / 加动作 |
| 模块开关 | 加载期禁用插件；运行期按字段过滤意图 | `tree_manifest.yaml` 的 `enabled`；`InterventionController::allows`（`kNavGoal→nav` 等） | 隔离测试 |

### 10.2 接口

用结构化 action / service，而不是裸 topic：

```text
# decision_msgs/action/ManualOverride.action
IntentField field      # nav.goal / resource.request / chassis.vel ...
string      value      # 类型化取值
float64     lease_sec  # 生效时长，到点自动撤销
string      reason     # 记录到日志与回放
---
bool   accepted
string message
---
bool   effective       # 当前是否在仲裁中胜出
string overridden_by   # 若被更高优先级覆盖，是谁
```

```text
# decision_msgs/srv/DebugCommand.srv
string command   # set_intent / clear_intent / set_world / disable_module / list_state
string json_args # 按 JSON Schema 校验
---
bool   success
string message
string state_json
```

强类型 action 面向实车与正式接管；通用 service 面向调试与网页面板。二者都转成 `Intent` 或世界状态覆盖，进入同一条流水线。

### 10.3 相对手动发 topic 的优势

| 维度 | 手动发 topic | 干预模块 |
| --- | --- | --- |
| 需要知道 | topic 名、QoS、消息字段、frame | 语义化字段名，可自动补全 / 生成表单 |
| 时间语义 | 一次性，无过期、无取消 | lease 自动失效 + action 可取消 |
| 冲突可见 | 不知道被谁覆盖 | 仲裁器逐字段给出胜负与原因 |
| 改世界 | 要自己造整套消息 | 只覆盖一个字段 |
| 回放 | 不在决策输入记录里，无法复现 | 作为输入通道一起录制 / 重放 |
| 自动化 | 难 | 场景脚本时间轴 |
| 安全 | 可能绕过安全 | 走同一仲裁，安全仍最高 |

### 10.4 网页面板

在 rosbridge 战场页中提供决策调试面板：左侧树状态、中间战场俯视图、右侧 `WorldState` 与活跃 Intent 列表
（来源 / 优先级 / 剩余 lease / 是否胜出）以及按钮组（强制撤退、去点位、切换姿态、买弹、禁用模块、清空手动意图、急停）。
面板根据 `list_state` 返回的 schema 自动生成表单，新增字段无需改前端。

### 10.5 回放与场景脚本

人工干预必须作为一路带时间戳的输入被记录，回放时按原时间点重放，否则「只靠回放复现」在有人干预时立刻失效。
同一机制也支持场景脚本，把干预变成可提交的测试用例：

```yaml
# scenario/retreat.yaml
- at: 5.0
  set_world:  { self.hp: 20 }
- at: 6.0
  add_intent: { field: chassis.vel, value: [0, 0, 0], lease: 2.0, source: intervention }
- at: 8.0
  disable:    [tactical]
- at: 12.0
  expect:     { nav_goal: point_home, tactical_mode: retreat }
```

### 10.6 安全边界

干预模块是受控入口，不是绕过安全的后门：它进入仲裁器，而不是直接写 `DecisionOutput`；
所有干预记 `ACT` 日志并进回放；比赛模式下可关闭调试专用能力。

P2 范围：先落地 core 侧的三类注入（意图注入、世界状态覆盖、模块开关）；结构化 action / service
与网页面板由 P3 落地，设计见 §14.3 与 §14.6。

世界状态覆盖**只改数值、不提升 `referee.valid`**，因此不会绕过失效急停；模块开关分两级：加载期由
`tree_manifest.yaml` 的 `enabled` 决定 .so 是否加载，运行期由 `InterventionController::allows` 按字段过滤意图。

## 11. 导航模块

导航拆成决策与执行两个模块，分属不同层：

| 模块 | 层 | 包 | 职责 |
| --- | --- | --- | --- |
| `nav_policy` | 意图层（任务 / 技能） | nodes | 决定去哪个点、是否撤退、脱困策略，产生 `nav_goal` Intent |
| `nav_executor` | 指令 / 执行层 | io | 直接用 `nav2_msgs/action/NavigateToPose` 自研薄封装（不依赖 `nav2_behavior_tree` / BehaviorTree.ROS2），跟随仲裁后的 goal 并上报进度与当前点 |

闭环：`nav_executor` 把「已到达 / 失败 / 当前点 / 剩余距离」写回 `WorldState`，下一 tick `nav_policy` 读取后再决策。
全系统只有一个活跃 `nav_goal`，因此它必须走仲裁。

P2 落地：`nav_policy` 表现为 `tree/mission/nav/`（任务选择）+ `tree/skill/`（去点、巡逻、脱困，产生 `kNavGoal` Intent）；
`nav_executor` 直接在 `io` 中用 `nav2_msgs/action/NavigateToPose` 跟随仲裁后的目标，并把进展回写 `NavState`。

## 12. 日志与可观测性

日志分级与双通道：

| 级别 | 含义 | 完整日志 | 简短日志 + stdout |
| --- | --- | --- | --- |
| DEBUG | 开发调试 | 是 | 否 |
| INFO | 运行状态 | 是 | 否 |
| ACT | 重要决策行为（模式切换、目标变更、分支抢占、资源请求） | 是 | **是** |
| WARN | 潜在问题 | 是 | 是 |
| ERROR | 异常 | 是 | 是 |

- 统一使用项目日志接口，禁止散落的 `std::cout` 与原始 `RCLCPP_*`；
- 状态转移日志只在「上次状态 ≠ 本次状态」时打印，避免逐 tick 刷屏；
- 高频回调禁止逐帧 INFO / DEBUG；
- 发布 `DecisionState` / `WorldState` 到 `/decision/state`、`/decision/world_state`，`TreeStatus` 到 `/decision/tree_status`，供可视化与 rosbag 记录；Groot2 为可选接入（见 §14）。

## 13. 回放与测试

**回放契约**：凡决策读到的输入、发出的输出、以及人工干预，都必须能记录并用同一份 core 确定性重放
（`use_sim_time`、固定 tick 顺序、固定随机种子）。

核心实现：`core/replay.hpp` 定义与 ROS 解耦的 `ReplayData`（带仿真时间戳的输入记录）与 `ReplaySource`
（固定步长推进、按时间取最近记录、时间戳 = `epoch + at`）。`ReplaySource` 同时实现 `RefereeSource` /
`OdometrySource` / `NavigationSink`，可直接驱动 `WorldModel`，并统计决策下发的目标数用于回归断言。
rosbag 读取由 io 适配器 `load_replay_data`（`rosbag2_cpp`）负责填充 `ReplayData`，默认映射旧仓库
的简单话题（`/ifhealth`、`/remain_ammo`、`/our_base_health`、`/our_outpost_health`、
`/enemy_outpost_health`、`/can_rebuild_outpost`、`/odom`）；core 回放不依赖任何 ROS 类型，确定性由宿主单测覆盖。

测试金字塔：

1. **core 单测**（无 ROS）：裁判协议位段解码、几何、仲裁优先级 / lease、配置校验。
2. **BT 节点单测**：用假 `WorldState` / 假接口，断言 `NodeStatus` 与产生的 `Intent`。
3. **树级表驱动测试**：给定 `WorldState` → 期望 `DecisionOutput`，防止改 XML 造成回归。
4. **回放回归**：录制的比赛数据离线重跑，与录制的决策输出逐 tick 对比，生成差异报告。
5. **仿真集成**：与 Gazebo / 裁判仿真闭环联调。

## 14. 可视化与仿真

目标：让决策过程可观测、可手动干预、可赛后回放（对应 `docs/ROADMAP.md` P3）。
可视化只**读取**决策数据，干预只经受控通道写入，二者都不参与决策、不绕过安全。

### 14.1 包与边界

| 组件 | 包 | 职责 |
| --- | --- | --- |
| `TreeStatePublisher` | `sentry_decision_viz` | 采集 BT 节点状态并发布 `/decision/tree_status` |
| Groot2 桥 | `sentry_decision_viz` | 可选地把行为树挂到 `BT::Groot2Publisher`，供 Groot2 实时连接 |
| 网页面板 | `sentry_decision_viz/web` | 纯静态页：树状态、战场俯视图、WorldState / Intent、干预按钮 |
| `referee_sim_node` | `sentry_decision_sim` | ROS 侧裁判仿真：发 `/sentry/*` 与 odom，跑导航 action server 与动作回执 |
| `InterventionServer` | `sentry_decision_io` | 干预 action / service 服务端，转成线程安全命令队列 |

`sentry_decision_viz` 依赖 `core`、`sentry_decision_msgs` 与 `behaviortree_cpp`，
只读决策数据，不被决策主循环依赖。core 的仿真组件保持 ROS 无关，`referee_sim_node` 只是薄驱动。

### 14.2 树状态（TreeStatus）

行为树 tick 在单线程执行，可视化通过只读快照观察。BT.CPP v4 提供 `Tree::applyVisitor` 与
`TreeNode::status()/fullPath()/registrationName()`，据此展平整棵树：

- `sentry_decision_msgs/msg/TreeNodeStatus`：`full_path`、`registration_name`、`instance_name`、
  `status`（与 `BT::NodeStatus` 1:1：IDLE 0 / RUNNING 1 / SUCCESS 2 / FAILURE 3 / SKIPPED 4）。
- `sentry_decision_msgs/msg/TreeStatus`：`header`、`tick`、`tick_ms`、
  `TreeNodeStatus[] nodes`、`string[] active_path`。
- **active path** = 所有 `RUNNING` 节点的 `full_path`：BT 中运行中节点的祖先必然也在运行，
  因此等价于「RUNNING 及祖先链」。前端据此高亮当前执行分支。
- 话题 `/decision/tree_status`，QoS 使用 transient local，保证后到订阅者能拿到最近一帧。

发布与 `DecisionState` 同一节拍（默认 20 Hz）且在 tick 末尾，因此树状态与当拍输出一致。
Groot2 为**可选**能力：`decision_node` 提供 `--groot2-port`，仅在显式开启时附加
`BT::Groot2Publisher`（BT.CPP 4.9 已带 zmq 支持），不作为 CI 验收项。

### 14.3 干预接口（ROS）

结构化 action / service 语义见 §10.2，消息落在 `sentry_decision_msgs`。这部分的关键是线程模型：
BT tick 跑在定时器回调，而 action / service 回调在 `MultiThreadedExecutor` 的其他线程，
不能直接共享 `InterventionController`：

```text
Action / Service 回调线程 --> 加锁命令队列 --> tick 边界 drain --> InterventionController
```

- 回调只做**校验 + 入队**，不触碰 `WorldState` 与行为树；
- tick 开始时按固定顺序应用本拍命令，保证确定性；
- 应用成功的干预记 `ACT`（来源、字段、lease、原因），并发布到 `/decision/intervention` 以便录制与回放；
- `IntentArbiter::resolve` 的结果新增逐字段胜者 `winners`，供 action 反馈与 `list_state` 使用。

**action `ManualOverride`**（goal / result / feedback）：

| goal 字段 | 说明 |
| --- | --- |
| `field` | `FIELD_NAV_GOAL` / `FIELD_CHASSIS_VEL` / `FIELD_RESOURCE_REQUEST` / `FIELD_TACTICAL_MODE`，与 `core::IntentField` 一致 |
| `value` | 类型化文本（YAML/JSON）：`[x, y, yaw]` / `[vx, vy, wz]` / `{ammo, hp, revive}` / 模式名或 0-6 |
| `lease_sec` | 生效时长，`0` 表示不过期；goal 保持执行态直到失效或被取消 |
| `reason` | 记入日志与 `/decision/intervention` |

result 返回是否接受；feedback 每 tick 给出该字段的 `effective` 与 `overridden_by`。

**service `DebugCommand`**（`command` + `args`，args 为 YAML/JSON 文本）：

| command | args | 作用 |
| --- | --- | --- |
| `set_intent` | `{field, value, lease_sec, reason}` | 同 ManualOverride，但同步返回 |
| `clear_intent` | `{field}` | 撤销某字段注入 |
| `set_world` | `{field, value}` | 覆盖世界数值 |
| `clear_world` | `{field}` 或省略 | 清除覆盖 |
| `set_module` | `{module, enabled}` | 运行期模块开关 |
| `clear_all` | — | 清空全部干预 |
| `list_state` | — | 返回 `world / intents / winners / modules / world_overrides / safety_emergency` 的 JSON 快照，供网页面板自动生成表单（§10.4） |

### 14.4 裁判仿真与场景脚本

`referee_sim_node` 复用 core 中 ROS 无关的仿真组件，把本地仿真接到真实 IO 路径上：

- 发布 `sentry_interfaces` 的五条上行消息与 odom，驱动 `decision_node`；
- 内嵌 `NavSimulator` 作为 `NavigateToPose` action server，并在收到 `DecisionCommand` 后用
  `DecisionActuatorSim` 回 `DecisionAck`，形成完整闭环；
- 场景脚本 YAML 描述带时间轴的事件与断言：

```yaml
# scenario/full_match.yaml（片段）
world:   { self_hp: 400, self_ammo: 100, enemy_outpost_hp: 1500 }
timeline:
  - at: 2.0
    expect:    { tactical_mode: patrol, has_nav_goal: true }
  - at: 3.0
    set_world: { game_time_remaining: 300 }
  - at: 6.0
    set_world: { self_hp: 40 }
  - at: 7.5
    expect:    { tactical_mode: retreat, nav_goal_x: -5.0, nav_goal_y: 3.0 }
```

P3.1 支持 `set_world` / `expect`；`add_intent` / `disable` 所需的干预通道已在 P3.2 就绪，
场景脚本接入留待后续。场景在 `colcon test` 中启动 `referee_sim_node` + `decision_node`，
订阅 `/decision/state` 在事件时刻断言，等价于 §10.5 的「干预即测试用例」。

### 14.5 干预回放

干预是一路带时间戳的输入（§10.5），必须与信念输入一起录制、按原时刻重放：

- `core::ReplayData` 增 `interventions` 通道，`ReplaySource` 暴露当前时刻生效的干预事件；
- io 的 `load_replay_data` 从 `/decision/intervention` 读取；
- 回放时干预事件在对应 tick 注入 `InterventionController`，因此含干预的回放仍逐 tick 确定。

### 14.6 网页面板

rosbridge + roslibjs 的纯静态页，**无打包 / 构建步骤**（Node 仅用于纯逻辑单测）：

- 布局：左侧行为树（`TreeStatus`，active path 高亮）、中间战场俯视图（己方 / 导航目标 /
  敌方，canvas 绘制）、右侧 `WorldState` 与模块 / Intent 面板；
- 干预按钮：强制撤退、切换模式、前往点位、兑换发弹 / 血量、模块启用 / 禁用、清空干预，
  分别调用 `/decision/debug`（service）与 `/decision/manual_override`（action）；
- 前端按 `bridge`（roslib 适配）/ `store`（订阅式状态）/ `format`（纯转换）/
  `panels/*`（每个面板一个模块）/ `battlefield`（canvas）分层，使用浏览器原生 ES modules；
- `web/vendor/roslib.min.js` 锁版本 vendor（BSD-2，1.4.1）；`web/package.json` 仅声明
  `type: module` 供 `node` 单测使用，无依赖；
- `viz.launch.py` 启动 `rosbridge_websocket`（默认 9090）与 `python3 -m http.server`
  （默认 8080，指向安装后的 `share/sentry_decision_viz/web`）；
- 纯逻辑（消息 → 视图模型、坐标变换）在 `web/test/format.test.mjs` 用 `node` 单测；
  `tools/viz_smoke_test.sh` 校验静态资源、rosbridge 端口与决策话题数据通路（未装 rosbridge 时跳过）；
- 不引入打包器：当前规模的收益大于成本，且分层已为将来平滑迁移到 Vite + TS 留好接缝；
- 说明：`WorldState` 目前只带单个敌方位置与队友数量，战场页暂不画队友；「急停」无独立接口，留待 P4。

### 14.7 依赖

- 镜像新增 `ros-jazzy-rosbridge-suite`；
- Groot2 依赖已随 `ros-jazzy-behaviortree-cpp`（4.9.0）提供，无需额外系统包；
- 网页面板仅新增一个 vendor 的 JS 文件，不引入前端工具链。

## 15. 配置

- 保留并强化 profile：地图 profile、策略 profile、赛前开关。
- **单一入口 + 按职责分文件**：

```text
config/
├── profiles.yaml           # 唯一入口：map_profile / strategy_profile / pre_match
├── maps/<MAP>.yaml         # 命名点、区域、frame
└── policies/<POLICY>.yaml  # 阈值、时间窗、冷却、兑换步长
```

- `core` 只定义纯结构 `PolicyConfig`（不依赖 YAML）；`bringup` 用 `yaml-cpp` 解析为结构并做校验。
- XML 不写数值，坐标用命名点、阈值用配置 key；key 在节点构造时解析，缺失即启动失败。
- 所有 YAML 做必填 / 类型 / 范围校验：数值必须完整解析；已知数值键按类型表要求整数且非负。
  加载失败启动即报错并列出可用 key；启动时把生效配置打印为 `ACT` 日志 / 落盘，便于复盘。

## 16. 运行环境与部署

- 目标运行环境：Ubuntu 24.04 + ROS 2 Jazzy。
- 开发宿主若非 24.04，统一通过 Docker 开发与测试。
- 当前阶段无法上实车，所有开发以本地 PC 容器验证为准；镜像、入口与 Compose 见 `docker/`。
- 本地仿真：`sentry_decision_sim` 提供 dummy 裁判系统 `RefereeSimulator` 与伪导航 `NavSimulator`，
  直接实现 core 的 IO 端口并驱动信念层，因此无需下位机通信包与 navigation 仓库即可调试。
- 固定源码依赖（vcstool）与二进制依赖（rosdep），去掉临时的 clone + patch。
- 采用 Docker Compose 与已验证的自定义 bridge 网络（不使用 host 网络），构建产物挂载到宿主避免重复编译。
- 24.04 实车与容器使用同一套依赖描述，保证一致。

## 17. 开放问题

- 对外接口冻结清单（`/sentry/behaivor_send`、`/set_bool`、`/change_follow_mark` 等）。
- 下位机通信包（上行 / 下行帧）的定义位置与字节协议；确认后再落地 `UplinkFrame` / `DownlinkFrame`。
- 可视化选型已定：`TreeStatePublisher` + 可选 Groot2 + rosbridge 静态页（见 §14），细节随 P3 落地调整。
- 回放文件格式与录制范围。
- BT.CPP 在 Jazzy 镜像中的具体版本锁定与语义复验。
