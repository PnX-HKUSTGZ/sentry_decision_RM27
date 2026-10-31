# 开发笔记

> 短期临时文档：只记录**当前 sprint** 的进度、已锁定决策与 TODO，不保留历史。
> 历史变更见 [CHANGELOG.md](CHANGELOG.md)，阶段状态见 [ROADMAP.md](ROADMAP.md)。
> 仅作为开发草稿纸，不属于项目正式文档。

## 当前 sprint：v0.5.1 通信简化（取消回执）

### 目标

MCU 只把决策转成裁判 `0x0120` 指令，不回执、不判断成功；决策侧用裁判 `0x020D` 上行校验是否生效，
未生效则重发同一目标，超过次数上限后放弃。

### 已锁定决策

- **累计目标值**：依据 2027 通信协议 V1.0.0，0x0120 的兑换字段是「本场累计值」
  （发弹量 bit 2-12、远程次数 bit 13-16 / 17-20），重复下发不会重复扣费。
  决策层仍以增量表达意图，由 `ActionDispatcher` 在发送时折算成累计目标值，并保证同一局内单调不减。
- **取消回执**：删除串口 `type=0x03` 与 ROS `DecisionAck`；串口下行从 12 B 缩为 9 B
  （`request_id` + `kind` + `value`）。
- **生效判据**：本地兑换看 `info1.ammo_exchanged_local`；远程兑换看对应累计次数；
  复活看 `self_hp` 从 0 变正（协议没有「已确认复活」回传）。
- **重试参数**：`action.verify_delay_ms`（默认 600）、`action.max_attempts`（默认 3）；
  复活用 `revive_verify_delay_ms`（200）与 `revive_max_attempts`（0 = 不限）。
- **电平位**：确认复活 / 立即复活是 0x0120 bit 0 / 1；待复活期间持续置 1，
  完成或放弃后补发 `value=0` 清零。MCU 原样保持电平，不做超时判断。
- **删除本地兑换血量**：0x0120 没有对应字段，`kHpExchange` / `resource.hp` 一并移除。

### 已完成

- `ActionDispatcher`：发送 → 等待 → 校验 → 重试 / 放弃；上游无效时暂停；电平位自动清零。
- 接口：删 `DecisionAck.msg`，`DecisionAction` 只剩 `kind` / `value`；`DecisionOutput` 去掉 `resource_hp`。
- 仿真：`DecisionActuatorSim` 不再产生回执，动作直接结算进 `SimWorld`；执行端按累计值幂等结算。
- 面板：`last_ack` 改为 `last_action_report`（生效 / 放弃、尝试次数）。
- 文档：`INTERFACES.md`、`ARCHITECTURE.md` §4.1 / §14、`BELIEF.md`、`ROADMAP.md`、
  `CHANGELOG.md`、auto-aim `serial_protocol.md` / `decision_interface.md` 同步。

### 待办

- **MCU 侧实现协议 v1**：裁判上行 / 决策下行、0x0120 状态合并、单调性与电平语义；见
  `serial_protocol.md` §2.4 / §3。
- `SentryInfoOffline` 视觉锁定字段（`is_get` / `armor_pos` / `armor_num`）与
  `TeamInfo.allies[4]` 接入 `EnemyState` / `WorldState.allies`。
- 实机验证：决策 ↔ auto-aim 的 ROS 通信（不接 MCU 可用 `referee_sim_node` 扮演上行；
  见 [SIMULATION.md](SIMULATION.md) §3）。
- 旧 rosbag 逐 tick 差异报告（等可用样本）。

## 注意事项

- 不自动执行 Git 操作（`add` / `commit` / `push`），先征求开发者确认。
- 不自动执行系统级操作（安装软件 / 改系统配置），应放进 Dockerfile 或显式脚本。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 参数一律外置到 `config/` 下的 YAML，源码不写可调默认值，缺失即启动失败。
- 未确定项集中记录在 [ARCHITECTURE.md](ARCHITECTURE.md) §17。
