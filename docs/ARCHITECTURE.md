# 项目架构（RMUC 2027）

> 本文是 2027 Task 层的当前架构入口。旧 2026 行为树 / Mission / TacticalMode 架构已归档到
> [archive/ARCHITECTURE_2026.md](archive/ARCHITECTURE_2026.md)，不再参与构建。
>
> 任务细节见 [TASK_CATALOG.md](TASK_CATALOG.md)，战略层设计见 [STRATEGY_LAYER.md](STRATEGY_LAYER.md)，
> 命名点见 [POINTS_2027.md](POINTS_2027.md)，上位机接口见 [INTERFACES.md](INTERFACES.md)。

## 1. 分层

```text
RosIoNode / WorldModel
        │ WorldState
        ▼
TaskRunner
  ├─ RuleBasedTaskPolicy   （纯函数：选一个原子 Task）
  └─ TaskRuntime           （有状态：生命周期 / 超时 / 抢占 / 结果）
        │ TaskOutput
        ▼
NavGoalTracker / ActionDispatcher
        │
        ▼
SafetySupervisor
        │ DecisionOutput
        ▼
DecisionCommand → auto-aim → MCU
```

- `Task` 是原子行为，不内嵌其他 Task；组合目标由动作层连续选择。
- `TaskRunner` 是组合根，不直接接触 ROS。
- `TaskState` 是下行执行状态，不替代具体动作。
- 旧 `IntentArbiter` / `TacticalMode` / `Mission` 树已删除。

## 2. 任务

| Task | 作用 |
|---|---|
| `Navigate` | 导航到命名点 |
| `Scan` | 原地扫描，不自动结束 |
| `Outpost` | 只下发 `TaskState=kOutpost`，由自瞄攻击 |
| `Heal` | 导航到基地 / 己方完全控制前哨站并回血；基地确认免费发弹 |
| `Resource` | 状态型买弹，5 s 等累计值到账 |
| `Respawn` | 只等待免费复活 |

所有 Task 不做内部重试；失败 / 超时回到 `TaskPolicy`。

## 3. 关键状态

- `WorldState`：`OutpostControl`、`supply_claimed_minutes` 等。
- `TaskSnapshot`：当前任务、`call`、`TaskState`、阶段、进度、metrics。
- `TaskResult`：成功 / 失败 / 超时 / abort，只保留上一次给 `HistoryView`。
- `TaskPlan.valid_for`：计划新鲜度（默认 1 s）。
- `TaskCall.timeout`：任务执行上限，0 = 不超时。

## 4. 通信与协议

- 上行：`GameInfo` / `SentryInfoOnline` / `SentryInfoOffline` / `TeamInfo`。
- `GameInfo.event_code` + `event_code_2` 走 2027 `0x0101` 解码；R/B 由 `own_color` 映射。
- 下行：`DecisionCommand` 承载 `task_state` + `ActionRequest`。
- auto-aim ↔ MCU：33 B 上行 / 10 B 下行；MCU 固件仍待同步。
- 动作只发一次，不重试；兑换任务用 5 s 检测窗口。

## 5. 包结构

| 包 | 职责 |
|---|---|
| `sentry_interfaces` | auto-aim ↔ 决策 ROS 合同 |
| `sentry_decision_msgs` | 决策内部状态消息 / service |
| `sentry_decision_core` | 信念、Task、动作、安全、日志，纯逻辑 |
| `sentry_decision_io` | ROS IO、状态发布、面板 service |
| `sentry_decision_sim` | 裁判 / 导航 / 执行端仿真 |
| `sentry_decision_viz` | 状态可视化与网页面板 |
| `sentry_decision_bringup` | `task_node` 组合根与配置 |

已删除：`sentry_decision_nodes`、`tree/`、旧 `decision_node` / `decision_main` / `replay_main`。

## 6. 入口

```bash
ros2 run sentry_decision_bringup task_node
# 或
ros2 launch sentry_decision_bringup task.launch.py
```

`task_node` 运行 `TaskRunner + RosIoNode + SafetySupervisor`；配置入口为 `config/profiles.yaml`。

## 7. 测试

宿主纯逻辑测试：

```bash
tools/host_core_test.sh
```

覆盖 TaskRuntime、TaskPolicy、TaskRunner、六个 Task、事件解码、动作派发、安全、信念等。
ROS 侧用 `colcon test` 跑 IO / sim / 启动冒烟。
`tools/format.sh --check` 必须通过。
