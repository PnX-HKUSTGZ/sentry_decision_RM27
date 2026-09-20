# 上位机接口契约（sentry_interfaces）

> 本文档与 `src/sentry_interfaces/msg/` 是「auto-aim ↔ 决策」接口的单一事实来源。
> 范围：只定义**裁判上行 / 决策下行**这一条链路，不改动 auto-aim 内部的云台控制帧（`VisionToGimbal`）
> 与导航接口（`NavToGimbalV2` / `/cmd_vel`）。串口字节布局的最终版本需与 MCU/电控确认。

## 1. 目标

- 上行：把裁判与自身/队友/雷达信息，从 auto-aim 统一成结构化消息发给决策，取代原来「一个标量一个话题」。
- 下行：把决策动作统一成结构化命令发给 auto-aim，由 auto-aim 经串口转给 MCU。
- 回执：MCU 执行结果经 auto-aim 以 `DecisionAck` 回传决策。
- 文档化：字段、单位、来源、版本、校验、语义集中在这里。

## 2. 分层与依赖

```text
MCU  <--串口(USB-CDC)-->  auto-aim (io::Gimbal + 串口帧)
                               │
                     sentry_interfaces (ROS 消息)
                               │
                         决策 (RosIoNode)
```

- auto-aim 拥有串口与 MCU 网关；`sentry_interfaces` 是 ROS 合同；决策只依赖该合同。
- 本阶段接口包放在决策仓库 `src/sentry_interfaces`，未来如需可独立成仓库。

## 3. 串口帧（版本化 + CRC）

统一帧格式，仅用于裁判/决策链路（低频率，控制帧不变）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `magic` | uint8 | 帧头，待与 MCU 约定 |
| `version` | uint8 | 协议版本，v1 = 0x01 |
| `type` | uint8 | 1 = RefereeUplink，2 = DecisionCommand |
| `length` | uint16 | payload 字节数 |
| `payload` | bytes | 版本化结构体 |
| `crc16` | uint16 | CRC16-CCITT，覆盖 `version..payload`，与 auto-aim `tools::get_crc16` 一致 |

**上行 payload `RefereeUplinkV1`**：裁判/自身/队友/雷达 + `DecisionAck`，频率建议 10~50 Hz。
**下行 payload `DecisionCommandV1`**：`request_id` + 单个动作（`kind/mode/interval/value`）。

带宽：只承载裁判/决策的低频数据，IMU/云台/导航仍走原有高频帧，115200 足够。

## 4. 上行 ROS 消息（auto-aim 发布）

| 话题 | 消息 | 关键字段 | 来源 |
| --- | --- | --- | --- |
| `/sentry/game_info` | `GameInfo` | 阶段、时间、金币、`event_code`、`detect_color`、`can_rebuild_outpost`、手动点、敌基地/前哨 | 裁判 |
| `/sentry/online_info` | `SentryInfoOnline` | 自身血量/弹量/热量/能量、`sentry_info_1/2/3` | 裁判 |
| `/sentry/offline_info` | `SentryInfoOffline` | 视觉锁定、装甲板、升降、变形、电容、隧道对齐 | 视觉/MCU |
| `/sentry/team_info` | `TeamInfo` | `allies[4]`、己方基地/前哨血量 | 裁判 |
| `/sentry/radar_info` | `RadarInfo` | `enemies[6]`、敌方经济、前哨感知 | 雷达 |
| `/sentry/decision_ack` | `DecisionAck` | `request_id`、`accepted`、`code`、`detail` | MCU 回执 |

`sentry_info_1/2/3` 与 `event_code` 是原始位段，由决策层用 `core/referee_protocol.hpp` 解码。
`detect_color` 编码待确认，当前留空。

## 5. 下行 ROS 消息（决策发布）

| 话题 | 消息 | 方向 |
| --- | --- | --- |
| `/sentry/decision_command` | `DecisionCommand` | 决策 → auto-aim |

`DecisionCommand`：`header` + `request_id` + `DecisionAction action`。

`DecisionAction`：

| 字段 | 说明 |
| --- | --- |
| `kind` | 动作类型，见常量（兑换发弹/血量、确认免费复活、立即复活、远程兑换） |
| `mode` | `MODE_ONE_SHOT` 或 `MODE_POLLED`，**配置动作时必须显式选择** |
| `interval_ms` | 仅 `MODE_POLLED` 使用，按此间隔重发，而非每帧重发 |
| `value` | 数量 / 次数，含义随 `kind` |

## 6. 动作语义

- **OneShot（边沿）**：决策侧只在「请求由无到有」时发一条命令；MCU 执行一次即完成，不应重复。
- **Polled（轮询）**：决策侧按 `interval_ms` 周期性发送当前期望值；MCU 以最新一条为准，停止发送即按超时失效。
- 每个 `DecisionCommand` 带唯一 `request_id`，MCU 执行后以 `DecisionAck` 回传同一个 `request_id`。

## 7. 回执（ack）链路

```text
决策 --DecisionCommand(request_id=N)--> auto-aim --串口--> MCU
MCU --串口--> auto-aim --DecisionAck(request_id=N, accepted)--> 决策
```

- 决策侧维护未确认命令表，超时未收到 ack 记 WARN（重发策略待定）。
- `DecisionAck.code` 的取值表待与 MCU 约定；0 为成功。

## 8. 与 auto-aim 现状的映射（Phase 1）

现有 `GimbalToVision` 可填的最小集合：

| 目标消息 | 现有字段 | 结果 |
| --- | --- | --- |
| `GameInfo` | `game_start`、`enemy_baseHP`、`enemy_outpostHP`、`can_rebuild_outpost`、`detect_color` | 部分 |
| `SentryInfoOnline` | `sentryHP`、`remain_ammo` | 部分 |
| `TeamInfo` | `our_baseHP`、`our_outpostHP` | 部分 |
| `SentryInfoOffline` | — | 全空 |
| `RadarInfo` | — | 全空 |
| `DecisionAck` | — | 待新增 |

其余字段需要扩展 MCU 上行帧；决策侧先按「有则填、无则默认」处理，不阻塞。

## 9. 迁移阶段

1. **接口定义**（本文档 + `sentry_interfaces` 消息）——本阶段。
2. **决策侧**：`RosIoNode` 订阅 5 个上行消息、发布 `DecisionCommand`、订阅 `DecisionAck`；先填现有字段。
3. **auto-aim 侧**：`Publish2DecisionMaking` 从 7 个标量迁到 5 个消息；新增下行订阅与串口打包。
4. **MCU 侧**：确定字节布局、动作集合、ack 编码；扩展固件。
5. **策略接入**：决策用 `DecisionCommand` 下发兑换/复活等。

## 10. 未决项

- `detect_color` 编码含义。
- 下行动作集合、`value` 语义、`code` 取值表（与电控/MCU 确认）。
- 串口帧的 `magic` / 字节布局 / 频率。
- 是否纳入更多上行字段（金币、能力机关等）。
