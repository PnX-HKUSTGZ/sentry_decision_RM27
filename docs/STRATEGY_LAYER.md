# 战略层设计（RMUC 2027）

> 状态：整体架构、Task 协议与串口字段已冻结；剩余高层问题见 §10.9。

## 1. 目标

- 移除「战略模式（TacticalMode）→ 固定任务树」的分类式结构。
- 战略层成为一个纯函数：输入信念/任务状态/宏观指令，输出 `TaskPlan`。
- 任务状态、生命周期、打断时机由 `TaskRuntime` 维护，战略层不持有运行时状态。
- 任务都是原子行为，不内嵌其他 Task；组合目标由动作层根据新状态连续选择。
- 指令层（IntentArbiter + SafetySupervisor）保持不变。

## 2. 分层

```text
[可选] MacroPlanner（远端 / 离线）
   输入：宏观快照 + 历史 + 事件
   输出：MacroDirective
   频率：5–20 s，事件可提前
        │
        ▼
Task Selection（动作层 / Strategy，纯函数）
   输入：WorldState + TaskSnapshot + HistoryView + MacroDirective
   输出：TaskPlan
   频率：事件触发；无事件时最长 1 s 触发一次
        │
        ▼
TaskRuntime（有状态）
   职责：接收/应用 TaskPlan，维护任务实例、tick、状态、超时、决策时机
   频率：10–20 Hz
        │
        ▼
Task（行为树子树 / 状态机）
   职责：封装导航、交战、买弹、复活等模块任务
        │ Intent
        ▼
IntentArbiter + SafetySupervisor（指令层）
   职责：字段级仲裁、限幅、互锁、安全否决
   频率：20 Hz
        │
        ▼
DecisionOutput
```

| 层 | 职责 | 输入 | 输出 | 频率 |
|---|---|---|---|---|
| MacroPlanner（可选） | 定方向：阶段、目标权重、风险、重点区域 | 宏观快照、历史、事件 | `MacroDirective` | 5–20 s |
| Task Selection | 定任务：选哪个 Task、参数、是否打断 | `WorldState`、`TaskSnapshot`、`HistoryView`、`MacroDirective` | `TaskPlan` | 事件触发 + 1 s 超时 |
| TaskRuntime | 定执行：tick、状态、抢占、超时、应用计划 | `TaskPlan`、`WorldState` | `Intent`、`TaskSnapshot` | 10–20 Hz |
| Command（下游指令层，不属于本战术层） | 定输出：字段级仲裁与安全 | `Intent`、安全状态 | `DecisionOutput` | 20 Hz |

> 本文的战术层 = MacroPlanner（可选）+ Task Selection + TaskRuntime/Task；Command 是下游指令层，列在此处只为说明交接关系。

## 3. 术语

| 术语 | 含义 | 例子 |
|---|---|---|
| Primitive / Atomic Action | 不可再分的执行原语 | `Move`、`Aim`、`Fire`、`BuyAmmo` |
| Task | 可被动作层直接选择的原子行为；不内嵌其他 Task | `Navigate`、`Scan`、`Outpost`、`Heal`、`Resource`、`Respawn` |
| TaskPlan | 一次选择的结果 | 当前只含一个主任务 `TaskCall` |
| Task Layer | Task 库 + TaskRuntime | 原 `tree/skill` + 执行器 |
| MacroDirective | 规划层给动作层的方向性约束 | 权重、风险、重点区域、允许任务 |

**Task Layer 不是原来的 Mission Layer。** 旧 Mission 是「模式 → 固定任务树」的分发；新 Task 是原子、可参数化、可被任意策略选择的行为库。

## 4. 各层职责

### 4.1 MacroPlanner（规划层）

- 可选：没有它时，动作层使用默认 `MacroDirective`。
- 输出宏观的方向性约束，不输出具体坐标或具体任务计划。
- 可以是 LLM、搜索、规则或大模型；只做慢速宏观指挥。

### 4.2 Task Selection（动作层）

- 纯函数，不持有任务状态、不 tick 技能、不直接执行。
- 输入：
  - 最新 `WorldState`；
  - 当前 active 任务的 `TaskSnapshot`（可为空）；
  - `HistoryView`（只保留上一次任务及结果）；
  - 可选 `MacroDirective`。
- 输出 `TaskPlan`。
- 第一版用 Utility AI：对 **Task** 打分，而不是对原子 Primitive 打分。
- 需要同时评估「继续当前任务」和「切换新任务」，不能只做 argmax。
- 允许输出：
  - 当前只输出一个主任务；
  - 资源任务放入主任务栏；远程兑换暂不实现；
  - 姿态 / 并行任务暂缓，不设并行栏。

### 4.3 TaskRuntime

- 有状态，是任务的唯一持有者。
- 职责：
  - 创建/启动/停止任务实例；
  - 每 tick 调用 `Task::tick(world)`；
  - 维护 `TaskStatus`、`phase`、`progress`、`elapsed`；
  - 保证同一时刻最多一个 active Task；
  - 执行任务级 `can_abort()` 与 `TaskCall.timeout`；
  - 在定时/事件发生时产生 `DecisionEpoch`；
  - 应用新 `TaskPlan`，halt 旧任务、启动新任务；
  - 收集 `Intent` 和 `TaskSnapshot`；
  - 记录计划、状态迁移、拒绝原因。
- 不负责：
  - 不选任务；
  - 不打分；
  - 不写 `DecisionOutput`；
  - 不做字段级仲裁；
  - 不做安全否决。
- 安全层可以绕过 `TaskRuntime` 直接强制中断；战略层的打断必须由动作层决定。
- 组合根 `TaskRunner` = `RuleBasedTaskPolicy` + `TaskRuntime` + 默认 Task 表；后续可替换 Policy，不动 Runtime。

### 4.4 Command（下游指令层，不属于本战术层）

- `IntentArbiter`：字段级仲裁、来源优先级、lease、限幅、互锁。
- `SafetySupervisor`：最高优先级 veto / 覆盖。
- `TaskRuntime` 只产生 `Intent`，不产生 `DecisionOutput`。

### 4.5 TaskState（下行）

- `TaskState` 是执行状态，不是旧的 `TacticalMode`。
- 由 `TaskRuntime` 根据当前 active Task 生成，随下行链路发送给 MCU / 执行层。
- `TaskState` 只表示“当前在做什么”；具体动作（资源兑换、复活确认等）仍走各自的 `ActionRequest`，不由 `TaskState` 代替。
- 下行：`DecisionCommand.task_state:uint8`；串口 `type=0x02` payload 10 B；变化即发 + 10 Hz 心跳，MCU 锁存。
- 取值与编码见 §10.1；`TaskState` 不进 `DecisionAction`，也不写 `0x0120`。

## 5. 仲裁与打断

| 问题 | 负责方 |
|---|---|
| 当前应该跑哪个 Task？ | Task Selection |
| 计划过期，继续还是重规划？ | TaskRuntime 发 `DecisionEpoch`，Task Selection 决定 |
| 任务失败，重试还是换任务？ | Task Selection（不重试，直接重新选 Task） |
| 两个 Intent 争同一个输出字段？ | IntentArbiter |
| 安全层要求急停/覆盖？ | SafetySupervisor |

### 5.1 并发

- 当前只有一个主任务栏，不再设 domain。
- 每个时刻最多一个 active Task；资源任务也在主任务栏执行。
- 姿态 / 并行栏暂缓；远程兑换暂不实现。
- 任务切换必须满足当前任务的 `can_abort()`；不设 `min_commit` / `lease`。

### 5.2 打断规则

- 默认允许打断。导航、交战等任务可以在任意时刻 abort 并重新规划。
- 每个 Task 只提供一个任务级 `can_abort()`；具体阶段是否真正可 abort 由 Task 内部判断，对外只报一个布尔。
- 特定任务不可中断，例如复活；这些任务声明 `can_abort=false`。
- 如果当前任务不能立即中止，`TaskRuntime` 延迟应用新计划，等任务到达可中止阶段或完成；安全层仍可强制覆盖。
- 动作层比较 `U_continue` 与 `U_switch`，加迟滞，避免抖动。

### 5.3 DecisionEpoch

`TaskRuntime` 在以下时机触发动作层决策：

- 无事件时，1 s 超时触发；
- 当前 Task 成功、失败、超时；
- 战场事件（基地受击、前哨被毁、自身低血、目标消失等）；
- `MacroDirective` 更新；
- 手动调试请求。

事件触发要合并/限频（建议 200–500 ms 最小间隔），避免团战中反复重规划。

## 6. Task 与行为树

- Task 的定义包含四部分：
  1. 传入参数（如命名点、时间上限）；
  2. 实际执行的 BT 子树；
  3. 完成回调（成功 / 失败 / 超时 / abort）；
  4. 可选的任务进行中状态上报。
- **Task 是原子的，不内嵌其他 Task。** BT 只组织任务内部步骤；跨任务的目标由动作层连续选择 Task 完成。
- `TaskRuntime` 负责创建任务实例、绑定 BT、接收完成回调；动作层不检查任务结束条件。
- 主任务完成后，Runtime 立即产生 `DecisionEpoch`，动作层选择下一个任务；原则上主任务栏不空闲。
- 例如 `Heal` 的内部步骤可以写成：

```text
Heal
  Sequence
    MoveToHealPoint(heal_point)     # 任务内导航原语
    WaitForFullHp
    WaitForSupplyAmmoClaimed
```

这不是调用 `Navigate` Task，也不是可复用的组合任务；只是 `Heal` 自己的 BT。
- 任务内部管理导航、瞄准、开火、换弹、失败条件；动作层只看任务级快照。
- `TaskSnapshot` 提供 `phase`、`progress`、`elapsed`、`metrics`（如目标血量、已消耗弹药）。
- 现有实现缺口：`EmitNavGoalFromPoint` 发完目标就返回 `SUCCESS`，任务层无法知道是否到达。需要改为有状态的任务（`StatefulActionNode` 或任务包装器），区分 `RUNNING / SUCCESS / FAILURE`。

## 7. Utility AI 的定位

- Utility AI 负责 **Task 选择**，不负责 Task 内部时序。
- 每个候选 Task 有：
  - `can_start(world)`；
  - `start_utility(world, directive)`；
  - `continue_utility(world, snapshot)`；
  - `abort_utility(world, snapshot)`。
- 选择逻辑：

```text
U_continue = utility(current_task, snapshot, world, directive)
U_switch   = max utility(candidate, world, directive)
if U_continue + hysteresis >= U_switch: keep current
else: switch
```

- 迟滞与紧急事件覆盖由 TaskRuntime 与任务参数共同保证；不设 `min_commit` / `abort_if`。
- Utility AI 是 baseline；后续可替换为学习效用 / 分层 RL，接口不变。

## 8. 日志与回放

不需要审计战略层内部推理，但必须记录可回放的结构化结果：

- `MacroDirective`（含来源、时间、TTL、置信度）；
- 每次 `DecisionEpoch`（原因、时间、当前任务）；
- `TaskSnapshot` 摘要；
- `TaskPlan`（任务、参数、`valid_for`、`timeout`）；
- 候选任务 top-k 分数与选择理由；
- 应用/拒绝原因、状态迁移、失败原因；
- 不要求记录 LLM 的完整思维链，只记录结构化 rationale。

## 9. 2027 与训练数据

- 只做 RMUC 2027，不兼容 2026 规则/地图。
- `refs/26-dataset` 是 26UC 区域赛数据，只能用于：
  - 宏观行为先验与阈值校准；
  - reward shaping / 评估指标参考。
- 不能直接作为 2027 的 TaskPlan 标签：无动作标签、全场状态而非局部观测、规则/地图不同。
- 2027 的训练与评估以 Arena-1 2027 自博弈/回放为主。

### 9.1 2027 Task 候选

各任务的参数、BT 子树、完成条件和协议定义见 [TASK_CATALOG.md](TASK_CATALOG.md)。

| Task | 类型 | 结束条件 | 备注 |
|---|---|---|---|
| `Navigate` | 主任务 | 到达定位点即结束 | 不处理占点；默认 `arrive_radius=0.2 m`、`timeout=30 s` |
| `Scan` | 主任务 | 不自动结束；由动作层主动切换 | 原地旋转；检测/开火由自瞄完成；Runtime 仍按事件/1 s 触发决策 |
| `Outpost` | 主任务 | 目标前哨站血量归零；进入时目标必为敌方占领 | 只下发 `TaskState=kOutpost`；不含导航；`target_side` 只表示位于己方/敌方半场 |
| `Resource` | 主任务 | 兑换累计值到账/失败 | 状态型，不导航；基地补给区或己方完全控制前哨站增益点；5 s 检测；一般 `Heal` 后选择 |
| `Heal` | 主任务 | 血量回满；基地补给区按需确认免费发弹到账 | 导航至基地补给区/己方完全控制前哨站；默认领取免费发弹；前哨站只回血 |
| `Respawn` | 主任务 | 免费复活成功 | 只等待免费复活；`Heal` 由动作层另选 |

> `Tunnel`、`Rune` 2027 第一版不做，不设 `TaskState`。姿态 / 强化姿态暂缓，不进入当前 Task 清单。

### 9.2 前哨站规则与策略背景

- 将敌方完全控制的前哨站转化为我方完全控制，有两条路径：
  1. 先占领前哨站增益点或中央高地增益点 5 s，转为临时控制，再击毁；
  2. 先击毁，转为敌方临时控制，再等待敌方离开且未占点 5 s。
- 己方完全控制的前哨站可作为补给区使用，但不能领取免费发弹量。
- 战略优先级：先保全己方半场的前哨站；己方前哨站完全控制且安全后，再尝试敌方前哨站。
- 对 Task 的影响：`Outpost` 只负责下发攻击状态和监测目标；`Heal` 需要根据控制状态选择基地补给区，或己方完全控制的任一前哨站（己方/敌方半场均可用）。
- `Heal` 在前哨站只回血：`claim_free_ammo` 默认 `true` 但在前哨站忽略，不领免费弹、也不因此失败。
- 基地补给区免费发弹是“存活且在区内”自动触发；`Heal` 只负责 5 s 到账确认，见 [TASK_CATALOG.md](TASK_CATALOG.md) §5.5。

## 10. 协议与字段

以下结构已冻结；剩余高层问题见 §10.9。

### 10.1 基础枚举

```cpp
using TaskId = std::string;  // 稳定 snake_case，如 Navigate / Heal

enum class TaskStatus : std::uint8_t {
  kIdle,
  kRunning,
  kSuccess,
  kFailure,
  kTimeout,
  kAborted,
};

enum class Side : std::uint8_t { kOwn, kEnemy };

// 下行 TaskState：只表示当前在做什么，不替代具体动作。
enum class TaskState : std::uint8_t {
  kIdle = 0,
  kNavigate = 1,
  kScan = 2,
  kOutpost = 3,
  kResource = 4,
  kHeal = 5,
  kRespawn = 6,
};

enum class ReplanReason {
  kPeriodic,
  kTaskFinished,
  kTaskFailed,
  kTaskTimeout,
  kEvent,
  kDirective,
  kManual,
  kPlanExpired,
};
```

下行协议（冻结）：

- `DecisionCommand` 增加独立字段 `task_state: uint8`，与 `action` 正交。
- 串口 `type=0x02` payload 从 9 B 变为 **10 B**：`request_id:uint32 | task_state:uint8 | kind:uint8 | value:int32`。
- `TaskState` 在变化时立即发送，并额外以 **10 Hz** 心跳重发；MCU 锁存最近值，不转发到 `0x0120`。
- 非 `kRunning`、无 active Task 时发送 `kIdle`。
- `TaskState` 不进 `DecisionAction`，避免和兑换 / 复活动作混用。

### 10.2 任务参数 / TaskCall / TaskPlan

```cpp
struct NavigateParams {
  std::string target;                 // POINTS_2027.md 中的命名点
  double arrive_radius = 0.2;         // m
};
struct ScanParams {};
struct OutpostParams { Side target_side = Side::kEnemy; };
struct RespawnParams {};
struct HealParams {
  std::string heal_point = "own_base_supply";  // 或 own/enemy_outpost_buff
  bool claim_free_ammo = true;        // 仅基地补给区有效
};
struct ResourceParams {
  std::string exchange_point;         // own_base_supply / own_outpost_buff
  int amount = 0;                     // 10 的整数倍
};

using TaskParams = std::variant<
    NavigateParams, ScanParams, OutpostParams, RespawnParams, HealParams, ResourceParams>;

struct TaskCall {
  TaskId task;
  TaskParams params;
  Duration timeout{0};  // 任务执行上限；0 = 不超时
};

struct TaskPlan {
  std::uint64_t id = 0;
  TimePoint created_at{};
  Duration valid_for{1000};  // 计划新鲜度，默认 1 s
  std::optional<TaskCall> call;  // 当前只允许一个主任务
  std::string rationale;
};
```

- `TaskId` 与 `TaskParams` 的 variant 必须匹配；Runtime 启动前校验，不匹配直接拒绝。
- `TaskPlan.valid_for`：超过 `created_at + valid_for` 才应用视为过期，Runtime 丢弃并发 `kPlanExpired`，不启动任务。
- `TaskCall.timeout`：任务启动后计时；0 表示不超时。超时由 Runtime 结束任务并回调 `kTimeout`。
- 不再有 `TaskCall.lease` / `priority` / `domain`；当前只有一个主任务栏。

### 10.3 TaskSnapshot / TaskResult

```cpp
struct TaskSnapshot {
  TaskId task;
  std::optional<TaskCall> call;  // 供 Task Selection 继续评估
  TaskStatus status = TaskStatus::kIdle;
  std::string phase;       // 任务自定义阶段，如 Move / Wait / Confirm
  float progress = 0.0f;   // 0–1
  TimePoint started_at{};
  Duration elapsed{0};
  bool can_abort = true;
  std::string abort_block_reason;
  std::string detail;      // 当前阻塞 / 失败原因
  std::map<std::string, double> metrics;
};

struct TaskResult {
  TaskId task;
  TaskStatus status = TaskStatus::kIdle;  // 仅 kSuccess / kFailure / kTimeout / kAborted
  TimePoint finished_at{};
  Duration elapsed{0};
  std::string reason;
  std::map<std::string, double> metrics;
};
```

- `TaskResult` 由 Runtime 单独回调 / 记录，不塞进 `TaskSnapshot`。
- `TaskSnapshot` 只描述 active 任务；任务结束后立即失效。

```cpp
struct HistoryView {
  std::optional<TaskResult> previous_task;  // 只保留上一次任务及结果
};
```

- 不保留长历史、趋势或事件序列；需要回放时看日志。
- 当前 active 任务仍通过 `TaskSnapshot` 传入，不放进 `HistoryView`。

### 10.4 MacroDirective

```cpp
struct MacroDirective {
  std::uint64_t id = 0;
  TimePoint created_at{};
  Duration valid_for{0};
  std::string phase;       // early / mid / late / endgame
  std::map<std::string, double> weights;  // outpost/control/economy/defense
  double risk_appetite = 0.5;
  std::vector<std::string> focus_zones;
  std::vector<TaskId> allowed_tasks;
  int reserve_coins = 0;
  double confidence = 0.0;
  std::string rationale;
};
```

### 10.5 DecisionEpoch

```cpp
struct DecisionEpoch {
  ReplanReason reason;
  TimePoint at;
  std::optional<TaskId> task;  // 相关任务，可为空
};
```

### 10.6 Task 接口

```cpp
class Task {
 public:
  virtual ~Task() = default;
  virtual bool can_start(const WorldState& world) const = 0;
  virtual void start(const TaskCall& call) = 0;
  virtual TaskStatus tick(const WorldState& world) = 0;
  virtual bool can_abort() const = 0;
  virtual void abort() = 0;
  virtual TaskSnapshot snapshot() const = 0;
};
```

### 10.7 TaskRuntime 接口

```cpp
class TaskRuntime {
 public:
  void apply(const TaskPlan& plan, TimePoint now);
  void tick(const WorldState& world, TimePoint now);
  std::vector<Intent> take_intents();
  std::optional<TaskSnapshot> snapshot() const;
  std::optional<Point2D> nav_goal() const;
  std::optional<ActionRequest> action() const;
  std::optional<TaskResult> take_result();
  std::optional<DecisionEpoch> take_epoch();
  TaskState task_state() const;
};
```

### 10.8 Strategy 签名

```cpp
using Strategy = std::function<TaskPlan(
    const WorldState& world,
    const std::optional<TaskSnapshot>& active_task,
    const HistoryView& history,
    const MacroDirective* directive)>;
```

### 10.9 高层结论

1. 点位按己方视角固定，Task 层只看到 `own` / `enemy`，不出现红蓝。红蓝只由 IO / Belief 在解析 2027 `R/B` 事件位时使用一次；`TaskPlan` / `TaskState` / 命名点都不带颜色。
2. `HistoryView` 只保留上一次任务及结果，不保留趋势 / 长历史。
3. `TacticalMode` 完全删除：枚举、`IntentField`、`DecisionOutput.tactical_mode`、面板显示与相关测试一起移除；调试改用 `task_state`。
4. `Resource` 的兑换额度是策略问题，不影响协议；第一版由动作层显式传 `amount`，后续再做批量 / 金币 / 剩余额度策略。

### 10.10 已讨论结论

- `Command` 是下游指令层，不属于本战术层；本层只到 `TaskRuntime` / `Intent`。
- 动作层无固定周期：事件触发；无事件时最长 1 s 触发一次。`TaskRuntime` 仍为 `10–20 Hz`。
- 打断默认允许；仅特定任务不可中断，由任务级 `Task::can_abort()` 决定；不设 `min_commit` / `abort_if` / `lease`。
- 当前只有主任务栏；每时刻最多一个 active Task，资源任务也在主任务栏，远程兑换暂不做。
- 所有点位使用 [POINTS_2027.md](POINTS_2027.md) 的 11 个命名点；Task 层只看到 `own` / `enemy`，红蓝映射只在 IO / Belief 做一次。
- `HistoryView` 只保留上一次任务及结果；`TacticalMode` 完全删除，调试改用 `task_state`。
- 所有 Task 不做内部重试；失败/超时交给动作层。裁判兑换类任务只设 5 s 检测窗口，不重发。`ActionDispatcher` 已改为单次发送，旧重试字段已删除。
- `Scan` / `Outpost` 是同级、互斥的 state-only 主任务；`Outpost` 不含导航。
- `Respawn`：只等待免费复活；成功后结束，`Heal` 由动作层另选。整个任务保持 `TaskState = kRespawn`。
- `Heal`：基地补给区自动领取累积免费发弹（每满 1 分钟 100 发），5 s 内确认；`claim_free_ammo` 默认 true，前哨站忽略；前哨站（己方或敌方半场，只要己方完全控制）只回血。
- `Resource`：状态型任务，不导航；在 `own_base_supply` / `own_outpost_buff` 花金币兑换；只发一次，5 s 等 `info1.ammo_exchanged_local` 追上目标。
- `Tunnel`、`Rune` 2027 第一版不做。
- 2027 `0x0101`：`event_data` / `event_data_2` 解码；R/B 用 `own_color` 映射为 own/enemy；`GameInfo.event_code_2` 已加。
- `TaskState` 串口字段：`DecisionCommand.task_state:uint8`，payload 10 B；取值 0–6。
- `TaskPlan.valid_for` 只表示计划新鲜度（默认 1 s）；`TaskCall.timeout` 才是任务执行上限。
- 权值路线：手工 utility baseline → Arena 黑盒调参 → 专家 Task 选择排序学习（线性/小 MLP）。

### 10.11 开放问题

暂无设计层开放问题；协议与字段已冻结。实现待办见 [TASK_CATALOG.md](TASK_CATALOG.md) §8。