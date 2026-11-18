# 开发笔记

> 短期临时文档：只记录**当前 sprint** 的进度、已锁定决策与 TODO，不保留历史。
> 历史变更见 [CHANGELOG.md](CHANGELOG.md)，阶段状态见 [ROADMAP.md](ROADMAP.md)。

## 2027 Task 层（当前）

### 已完成

- 纯逻辑 `Task` / `TaskRuntime` / `TaskRegistry`：`sentry_decision_core`。
- `Navigate`、`Scan`、`Outpost`、`Respawn`、`Heal`、`Resource` 六个原子 Task 与宿主测试。
- `WorldState` 增加 `OutpostControl` 与 `supply_claimed_minutes`；点位常量见 `points_2027.hpp`。
- 2027 `0x0101` 事件解码：`EventCode` 按 `own_color` 映射 R/B；`GameInfo.event_code_2` 与 auto-aim 33 B 上行已接通。
- `TaskRunner` 组合根：`RuleBasedTaskPolicy` + `TaskRuntime` + 默认 Task 表；策略由 `DecisionEpoch` + 1 s 周期门控。
- `TaskRuntime` 启动前执行 `Task::can_start`。
- `task_node` 已接入 `TaskRunner` + `RosIoNode` + `SafetySupervisor`；默认 launch / demo 已切换。
- 旧 `TacticalMode` / Mission 树 / 行为树可视化 / `TreeStatus` / Groot2 已删除；旧资源与冲突协议字段已清理。

### 待办

- MCU 按 33 B 上行 / 10 B 下行实现 2027 协议，并锁存 `task_state` 心跳。
- `SentryInfoOffline` 视觉锁定字段（`is_get` / `armor_pos` / `armor_num`）与
  `TeamInfo.allies[4]` 接入 `EnemyState` / `WorldState.allies`。
- 实机验证：Task 层、Nav2、`Heal` 免费发弹确认、`Resource` 到账与复活确认。
- 2027 场地命名点已完成标定，`config/profiles.yaml` 默认使用 RMUC27。

## 注意事项

- 不自动执行 Git 操作（`add` / `commit` / `push`），先征求开发者确认。
- 不自动执行系统级操作（安装软件 / 改系统配置），应放进 Dockerfile 或显式脚本。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 参数一律外置到 `config/` 下的 YAML，源码不写可调默认值，缺失即启动失败。
