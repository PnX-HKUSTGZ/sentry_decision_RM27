# 2027 Task Catalog（草案）

> 状态：任务定义已冻结。已定义 `Navigate`、`Scan`、`Outpost`、`Respawn`、`Heal`、`Resource`；`Tunnel`、`Rune` 暂不做。  
> 关联：[STRATEGY_LAYER.md](STRATEGY_LAYER.md)、[POINTS_2027.md](POINTS_2027.md)。  
> 范围：RMUC 2027，哨兵决策端，当前只实现主任务栏。

## 0. 通用约定

### 0.1 任务输入

```text
TaskCall {
  task: TaskId,
  params: TaskParams,   // 类型化 variant，每个 Task 一个分支
  timeout: Duration,    // 0 = 不超时
}
```

`TaskCall` 的通用字段只有 `task` / `params` / `timeout`，定义见 [STRATEGY_LAYER.md](STRATEGY_LAYER.md) §10.2；本文只列任务特定参数。

### 0.2 任务运行

- `TaskRuntime` 按 `task` 查找 Task 定义，创建实例，绑定 BT 子树。
- 每个 tick 调用 BT；BT 返回 `RUNNING / SUCCESS / FAILURE`。
- 超时由 `TaskRuntime` 计时，超时后回调失败。
- 任务结束后，`TaskRuntime` 产生 `DecisionEpoch`，动作层立即选择下一个任务。
- 当前只允许一个主任务；不做姿态并行栏。
- 当前 Task 都是原子单元；多步目标由动作层根据新状态连续选择，不内嵌其他 Task。
- 所有 Task 仅在比赛状态 `kRunning` 时由 `TaskRuntime` 启动；任务定义不再重复该前置条件。
- 所有点位统一使用 [POINTS_2027.md](POINTS_2027.md) 的命名点，不直接传坐标。
- 所有 Task 不做内部重试；失败/超时直接回调，由动作层下一轮重新决策。裁判兑换类任务只设置检测窗口（统一 5 s），只等待、不重发。
- 回合/比赛未处于 `kRunning` 时，`TaskRuntime` 不启动任何 Task，并下发 `TaskState::kIdle`。

### 0.3 任务输出

- `TaskState`：下行执行状态，由 `TaskRuntime` 根据 active Task 生成。
- 具体动作仍走各自的 `ActionRequest` / 下行命令，不由 `TaskState` 代替。
- 可选状态上报：`TaskSnapshot`。

### 0.4 完成回调

```text
TaskResult {
  task: TaskId,
  status: SUCCESS | FAILURE | TIMEOUT | ABORTED,
  finished_at: TimePoint,
  elapsed: Duration,
  reason: string,
  metrics: map<string, double>,
}
```

默认约定：

- `can_abort` 默认 `true`。
- `timeout = 0` 表示不超时。
- 失败/超时都算任务结束，动作层下一轮重新选择。

## 0.5 前哨站规则与策略背景

- 将敌方完全控制的前哨站转化为我方完全控制，有两条路径：
  1. 先占领前哨站增益点或中央高地增益点 5 s，转为临时控制，再击毁；
  2. 先击毁，转为敌方临时控制，再等待敌方离开且未占点 5 s。
- 己方完全控制的前哨站可作为补给区使用，但不能领取免费发弹量。
- 战略优先级：先保全己方半场的前哨站；己方前哨站完全控制且安全后，再尝试敌方前哨站。
- `Heal` 在己方半场前哨站只回血，`claim_free_ammo` 必须为 `false`；传 `true` 时忽略，不领免费弹、不因此失败。

---

## 1. Navigate

### 1.1 目的

导航到指定定位点；到达即结束。  
不处理占点；若需要占点，下一轮动作层选择 `Scan` 或 `Outpost`。

### 1.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `target` | named point | 是 | 2027 地图命名点 |
| `arrive_radius` | double | 否 | 默认 0.2 m；到达判定半径 |
| `timeout` | Duration | 否 | 默认 30 s；0 = 不超时 |
| `task_state` | 固定 | — | `TaskState::kNavigate` |

### 1.3 前置条件

- `WorldState.self.valid`、`WorldState.nav.valid`。
- 目标点有效。

### 1.4 BT 子树

```text
NavigateTask
  Sequence
    EmitNavGoal(target)
    WaitNavResult(arrive_radius, timeout)
```

> 现有 `EmitNavGoalFromPoint` 发完目标就返回 `SUCCESS`，不能直接作为本任务。需要改成有状态节点，等待 `nav.reached` / `nav.failed`。

### 1.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | `nav.reached` 且距离 ≤ `arrive_radius` |
| FAILURE | `nav.failed`、目标无效、导航拒绝 |
| TIMEOUT | 超过 `timeout` 仍未到达 |

### 1.6 打断

- `can_abort = true`。
- abort 时取消导航目标（`NavGoalTracker` 的取消契约）。
- 被新任务抢占时，旧导航目标必须取消，不能残留。

### 1.7 输出与 metrics

- `TaskState = kNavigate`。
- metrics：`distance_remaining`、`reached`、`failed`。

### 1.8 已定

- `arrive_radius` 默认 **0.2 m**；需要更宽松时由动作层显式覆盖。
- 运行中不更新目标；换点由动作层切换到新的 `Navigate`。
- `timeout` 默认 **30 s**；超时直接失败，不重试。

---

## 2. Scan

### 2.1 目的

原地旋转扫描；检测和开火由自瞄完成。  
Scan 不自动结束，动作层可在事件或 1 s 超时后主动切换。

### 2.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `timeout` | Duration | 否 | 默认 0 = 无限 |
| `task_state` | 固定 | — | `TaskState::kScan` |

### 2.3 前置条件

- 机器人存活、未处于不可旋转状态。

### 2.4 BT 子树

```text
ScanTask
  Sequence
    SetTaskState(kScan)
    MonitorWhileRunning
```

### 2.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | 不自动成功 |
| FAILURE | 无法旋转/执行层报错（可选） |
| TIMEOUT | 仅当显式设置 `timeout` |

Scan 通常以 **被动作层抢占/abort** 结束。

### 2.6 打断

- `can_abort = true`。
- abort 时停止旋转、停止扫描状态。
- 是否停止开火由自瞄/执行层决定，决策端只切状态。

### 2.7 输出与 metrics

- `TaskState = kScan`。
- metrics（供动作层判断是否切走）：
  - `enemy_seen`
  - `last_seen_age`
  - `threat_count`
  - `nearest_enemy_distance`
  - `ammo`
  - `hp`

### 2.8 实现约定

- 旋转、检测、开火全部由执行层/自瞄控制；不接收 `yaw_rate`。
- 发现敌人后保持 Scan，直到动作层切换。
- 不限制旋转角度范围/回中。

---

## 3. Outpost

### 3.1 目的

只下发 `TaskState = kOutpost`，由自瞄层尝试并击打目标前哨站。  
**不包含导航**；需要换射击位时，由动作层先选 `Navigate`。  
`Outpost` 与 `Scan` 同级、互斥选择，不并发执行。  
`target_side` 只表示位置：`own` = 己方半场内的前哨站，`enemy` = 敌方半场内的前哨站。进入 `Outpost` 时，目标前哨站一定正在被敌方占领，因此它是敌方建筑；此状态下自瞄只攻击该前哨站，不处理地面机器人。

### 3.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `target_side` | `own` / `enemy` | 是 | 只表示位置：`own` = 己方半场，`enemy` = 敌方半场；进入时目标必为敌方占领 |
| `timeout` | Duration | 否 | 0 = 不超时 |
| `task_state` | 固定 | — | `TaskState::kOutpost` |

### 3.3 前置条件

- 机器人存活。
- 目标前哨站存在，且 `target_side` 对应的血量/控制状态可读。
- 目标前哨站当前必须被敌方占领；若发现己方已控制，立即返回失败。

### 3.4 BT 子树

```text
OutpostTask
  Sequence
    SetTaskState(kOutpost)
    MonitorOutpost(target_side)
```

> 不导航、不选射击位；自瞄负责目标选择与开火。

### 3.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | 目标前哨站 `hp <= 0` |
| FAILURE | 目标当前未被敌方占领（含己方已控制）；目标无效；执行层报错 |
| TIMEOUT | 超过 `timeout` 仍未结束 |

> 任务在目标前哨站血量归零时立即结束；控制权转化（占点/等待 5 s）由后续任务或动作层处理。

### 3.6 打断

- `can_abort = true`。
- abort 时清除 `TaskState`，并让自瞄停止追踪该前哨站。
- 不涉及导航取消。

### 3.7 输出与 metrics

- `TaskState = kOutpost`。
- metrics：`outpost_hp`、`outpost_owner`、`control_state`、`distance`、`ammo`。

### 3.8 实现约定

- 控制权转化由动作层决策；`Outpost` 不内嵌占点/等待逻辑。
- `Outpost` 结束后，由动作层根据新状态选择下一个原子任务。

---

## 4. Respawn

### 4.1 目的

阵亡后：只等待免费复活；复活成功即结束。
### 4.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `timeout` | Duration | 否 | 0 = 不超时 |
| `task_state` | 固定 | — | `TaskState::kRespawn` |

### 4.3 前置条件

- 机器人阵亡，或已进入复活流程。

### 4.4 BT 子树

```text
RespawnTask
  WaitForFreeRevive
```

### 4.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | 免费复活成功（机器人重新存活） |
| FAILURE | 免费复活不可用、执行层报错 |
| TIMEOUT | 超过 `timeout` 仍未复活 |

### 4.6 打断

- 等待免费复活期间：`can_abort = false`（没有可执行动作）。
- 复活成功即任务结束，不包含后续导航/回血。
- 安全层可强制覆盖。

### 4.7 输出与 metrics

- 整个 `Respawn` 期间保持 `TaskState = kRespawn`。
- metrics：`alive`、`respawn_progress`。

### 4.8 实现约定

- 免费复活不可用时交由动作层策略处理；暂不考虑立即复活。
- 等待复活期间保持 `TaskState = kRespawn`。

---

## 5. Heal

### 5.1 目的

回到基地补给区，或己方完全控制的前哨站增益点；等待回满血。  
基地补给区同时领取累积的免费发弹量；前哨站只回血（速率为补给区的 50%）。  
`Heal` 也用于“发弹量不足时回基地补给区领取免费发弹量”。

### 5.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `heal_point` | named point | 否 | 默认 `own_base_supply`；只允许 `own_base_supply` / `own_outpost_buff` |
| `claim_free_ammo` | bool | 否 | 默认 `true`；仅基地补给区有效，前哨站忽略 |
| `timeout` | Duration | 否 | 0 = 不超时 |
| `task_state` | 固定 | — | `TaskState::kHeal` |

### 5.3 前置条件

- 机器人存活。
- `heal_point` 只能是 `own_base_supply` 或 `own_outpost_buff`，且地图配置必须给出 `radius > 0`。
- `own_outpost_buff`：己方前哨站必须由己方完全控制；任务完成前失去完全控制立即失败。
- `own_base_supply`：存活机器人位于补给区即自动激活补给区增益点，裁判自动发放累积的免费发弹量；没有额外 `ActionRequest`。
- `claim_free_ammo=false` 只表示“不等待/不校验到账”，不会阻止裁判自动发弹。

### 5.4 BT 子树

```text
HealTask
  Sequence
    MoveToHealPoint(heal_point)        # 任务内导航原语，不调用 Navigate Task
    WaitForFullHp
    [WaitForSupplyAmmoClaimed(claim_free_ammo)]
```

### 5.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | 在目标点内；血量回满；基地补给区且 `claim_free_ammo=true` 时，已无待领取分钟 |
| FAILURE | 目标点不可用（含任务完成前前哨站失去完全控制）、导航失败、基地补给区 5 s 内未确认领取、超时 |
| TIMEOUT | 超过 `timeout` |

免费发弹量（2027 规则第 78 页）：

- 比赛每满 1 分钟累积 **100 发**；未领取部分可累积。
- `elapsed_s = 420 - game_time_remaining_s`（比赛时长默认 420 s）。
- `claimable_minutes = floor(elapsed_s / 60) - supply_claimed_minutes`。
- `supply_claimed_minutes` 由 `TaskRunner` 的信念跟踪维护：机器人在己方补给区内、观测到 `self_ammo` 正增量、且 `info1.ammo_exchanged_local` 未同步增加时，更新到当前分钟；跨 Task 保留。
- `claimable_minutes <= 0`：无待领取，立即通过。
- `claimable_minutes > 0`：进入补给区后 5 s 内等待 `supply_claimed_minutes` 追平当前分钟；未追平则失败。
- 不比较精确增量：开火会减少 `self_ammo`，只看“是否发生了领取”。

### 5.6 打断

- `can_abort = true`。
- abort 时取消导航、停止等待；已经获得的血量/弹量保留。
- 离开目标点后，等待进度按规则重置。

### 5.7 输出与 metrics

- `TaskState = kHeal`。
- metrics：`hp`、`ammo`、`distance`、`in_zone`、`outpost_control`、`supply_claimed_minutes`。

### 5.8 实现约定

- 到达目标点后持续等待至回满血；超时则失败。
- 基地补给区 5 s 领取确认窗口；前哨站只回血，不领取免费发弹量。
- `Heal` 不是组合 Task；内部导航是 BT 原语，任务结束后由动作层另选 `Resource`。

---

## 6. Resource

### 6.1 目的

在己方基地补给区或己方完全控制的前哨站增益点，向裁判系统**兑换**允许发弹量。
`Resource` 是状态型任务：**不导航**，要求机器人已经在兑换点内；通常由动作层在 `Heal` 结束后选择。
免费发弹量由 `Heal` 负责，`Resource` 只做花金币的本地兑换。

### 6.2 参数

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `exchange_point` | named point | 是 | 只允许 `own_base_supply` / `own_outpost_buff` |
| `amount` | int | 是 | 本次兑换发数；必须是 10 的整数倍，且不超过 `1000 - info1.ammo_exchanged_local` |
| `timeout` | Duration | 否 | 0 = 不超时 |
| `task_state` | 固定 | — | `TaskState::kResource` |

### 6.3 前置条件

- 机器人存活，且已在 `exchange_point` 区域内。
- `own_outpost_buff`：己方前哨站必须由己方完全控制；发送前失去控制则失败。
- `info1.ammo_exchanged_local + amount <= 1000`。
- `coins >= amount`（17mm 本地兑换 1 金币/发）。

### 6.4 BT 子树

```text
ResourceTask
  Sequence
    CheckInExchangeZone(exchange_point)
    SendAmmoExchangeOnce(amount)
    WaitAmmoExchanged(baseline + amount, 5s)
```

### 6.5 完成条件

| 结果 | 条件 |
|---|---|
| SUCCESS | `info1.ammo_exchanged_local >= baseline + amount` |
| FAILURE | 不在兑换点、`amount` 非法、超出 1000 上限、金币不足、发送前失去前哨站完全控制、5 s 内未到账、超时 |
| TIMEOUT | 超过 `timeout` |

### 6.6 打断

- `can_abort = true`。
- 已下发的兑换请求不撤回；abort 只停止等待，结果仍以裁判累计值为准。

### 6.7 输出与 metrics

- `TaskState = kResource`。
- metrics：`amount`、`baseline`、`target`、`current`、`coins`、`in_zone`、`outpost_control`。

### 6.8 实现约定

- 不导航；需要换点时由动作层先选 `Navigate` / `Heal`。
- 只发送一次，不重发；5 s 检测窗口内等 `0x020D` 累计值追上目标。
- 不使用 `ActionDispatcher` 的重试状态机。

---

## 7. 暂不做

| Task | 状态 |
|---|---|
| `Tunnel` | 2027 第一版不做；不设 `TaskState` |
| `Rune` | 2027 第一版不做；不设 `TaskState` |

> 以后恢复时新增 Task 定义与 `TaskState` 取值；不要预留未使用的枚举值。

## 8. 跨任务已定 / 待办

已定：

1. `TaskState` 由下行 `DecisionCommand` 的独立 `task_state: uint8` 字段承载；取值见 [STRATEGY_LAYER.md](STRATEGY_LAYER.md) §10.1。
2. `TaskResult` 独立回调，不塞进 `TaskSnapshot`。
3. `TaskPlan.valid_for` 只表示计划新鲜度；`TaskCall.timeout` 才是任务执行上限；不再使用 `TaskCall.lease`。
4. 任务参数用类型化 `std::variant`，不用 `map<string,double>`。
5. `can_abort` 是任务级布尔；任务内部阶段是否真正响应由实现决定，对外只报一个值。
6. 命名点见 [POINTS_2027.md](POINTS_2027.md)。

待办：

1. **2027 场地事件上行**：决策侧 `EventCode` 已改为 2027 布局，auto-aim 已转发 `event_code_2`；MCU 固件待按 33 B 上行实现，否则基地控制状态按 0 处理。
2. ~~移除重试逻辑~~：`ActionRequest` 已去掉 `retry_interval` / `max_attempts`，`ActionDispatcher` 改为单次发送，resource BT 端口、配置项与面板 `attempts` 已删除。
3. ~~完全删除 `TacticalMode`~~：枚举、旧 Intent/Arbiter、`DecisionOutput.tactical_mode`、Mission 树、面板覆盖与测试已删除；调试改用 `task_state`。
4. `HistoryView` 只保留上一次 `TaskResult`，不保留趋势 / 长历史。
