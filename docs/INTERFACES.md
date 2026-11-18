# 上位机接口契约（sentry_interfaces）

> 本文档与 `src/sentry_interfaces/msg/` 是「auto-aim ↔ 决策」接口的单一事实来源。
> 范围：只定义**裁判上行 / 决策下行**这一条链路，不改动 auto-aim 内部的云台控制帧（`VisionToGimbal`）
> 与导航接口（`NavToGimbalV2` / `/cmd_vel`）。串口字节布局的最终版本需与 MCU / 电控确认。

## 1. 目标

- **上行**：把裁判与自身 / 队友信息，从 auto-aim 统一成结构化消息发给决策，取代原来「一个标量一个话题」。
- **下行**：把决策动作统一成结构化命令发给 auto-aim，由 auto-aim 经串口转给 MCU。
- **不做回执**：MCU 只把动作转成裁判 `0x0120` 指令，不判断成功、不回传；决策侧用裁判 `0x020D` 上行
  校验是否生效。未生效只在任务内记录失败，回到动作层重新决策，**不重发**。下发值是累计目标值，单次下发天然幂等。
- **文档化**：字段、单位、来源、版本、校验、语义集中在这里。

## 2. 分层与依赖

```text
MCU  <--串口(USB-CDC)-->  auto-aim (io::Gimbal + 串口帧 v1)
                               │
                     sentry_interfaces (ROS 消息)
                               │
                         决策 (RosIoNode)
```

- auto-aim 拥有串口与 MCU 网关；`sentry_interfaces` 是 ROS 合同；决策只依赖该合同。
- 接口包放在决策仓库 `src/sentry_interfaces`，未来如需可独立成仓库。

## 3. 串口帧（auto-aim ↔ MCU）

串口帧 v1 的完整字节布局（上行裁判、下行决策、CRC、频率与待确认项）是 auto-aim 侧的
单一事实来源，见 auto-aim
[`docs/serial_protocol.md` §2](https://github.com/PnX-HKUSTGZ/auto-aim-new/blob/sentry-decision-interface/docs/serial_protocol.md)。
本文只记录与决策接口相关的约定：

| 帧 | 方向 | `type` | 对应 ROS 消息 |
| --- | --- | --- | --- |
| `RefereeUplink` | MCU → auto-aim | `0x01` | `GameInfo` / `SentryInfoOnline` / `TeamInfo` |
| `DecisionCommand` | 决策 → auto-aim → MCU | `0x02` | `DecisionCommand` |

- 统一帧：`[magic=0xAA][version=0x01][type][length uint16][payload][crc16]`，
  CRC-16/MCRF4XX 覆盖 `version..payload`，与 auto-aim `tools::get_crc16` 一致。
- 带宽：只承载裁判 / 决策的低频数据（裁判上行建议 10 Hz）；IMU / 云台 / 导航仍走原有高频帧，115200 足够。
- **现有高频控制帧不变**：`0x5A`（MCU → 上位机，54 B，含 IMU / 云台与裁判基础字段）、
  `0xA5` / `0xA6`（上位机 → MCU，云台 / 底盘）；旧 `0xA7`（`ifreload`）已移除。
- minco 导航栈的分帧协议（`navi_minco_bit/.../protocol.hpp`）数据体与我们的消息一一对应，可作参考，
  但其校验为逐 2 字节求和且 `check()` 未完成，不直接复用。

## 4. 上行 ROS 消息（auto-aim 发布）

| 话题 | 消息 | 关键字段 | 来源 |
| --- | --- | --- | --- |
| `/sentry/game_info` | `GameInfo` | 阶段、时间、金币、`event_code` / `event_code_2`、`detect_color`、`can_rebuild_outpost`、手动点、敌基地 / 前哨 | 裁判 |
| `/sentry/online_info` | `SentryInfoOnline` | 自身血量 / 弹量 / 热量 / 能量、`sentry_info_1/2/3` | 裁判 |
| `/sentry/offline_info` | `SentryInfoOffline` | 视觉锁定、装甲板、电容 | auto-aim 视觉 / 本机 |
| `/sentry/team_info` | `TeamInfo` | `allies[4]`、己方基地 / 前哨血量 | 裁判 |

- `sentry_info_1/2/3` 与 `event_code` 是原始位段，由决策层用
  `core/referee_protocol.hpp` 解码（已按 2027 通信协议 V1.0.0 的 0x020D 补齐）。
- `sentry_info_1` 的 bit 0-10 / 11-14 / 15-18 是**累计值**（本场累计兑换发弹量 / 远程兑换次数），
  是决策侧判断动作是否生效的依据。
- `detect_color` 为视觉 / MCU 字段，裁判与通信协议文档中未定义编码，仍待 auto-aim 确认，当前只存不判。
- 决策侧对每条上行字段的接收、单位与有效性判定见 [BELIEF.md](BELIEF.md)。
- 雷达 / 变形相关字段当前未启用，已从消息与代码移除。

## 5. 下行 ROS 消息（决策发布）

| 话题 | 消息 | 方向 |
| --- | --- | --- |
| `/sentry/decision_command` | `DecisionCommand` | 决策 → auto-aim |

`DecisionCommand` = `header` + `request_id` + `task_state` + `DecisionAction action`。

`DecisionAction`：

| 字段 | 说明 |
| --- | --- |
| `kind` | 动作类型，见常量（兑换发弹、确认免费复活、立即复活、远程兑换） |
| `value` | **0x0120 的累计目标值 / 电平位**，不是本次增量（见 §6） |

`task_state`：

| 值 | 含义 |
| --- | --- |
| 0 | `kIdle`：非比赛中 / 无 active Task |
| 1 | `kNavigate` |
| 2 | `kScan` |
| 3 | `kOutpost` |
| 4 | `kResource` |
| 5 | `kHeal` |
| 6 | `kRespawn` |

`task_state` 变化时立即发送，并以 10 Hz 心跳重发；MCU 只锁存，不写入 `0x0120`。
`action.kind=0` 表示本帧只更新 `task_state`，不携带动作。
`request_id` 只用于日志关联；每个请求只发一次，不复用 id 做重试。

## 6. 动作语义

裁判协议本身按「总量」结算：0x0120 bit 2-12 是「哨兵将要兑换的发弹量值」，从 0 改到 X 消耗 X 金币；
同一个 X 重复下发不会再扣一次。因此决策侧下发的是累计目标值：

| `kind` | 动作 | 决策层增量 | 下发 `value` | 生效判据（0x020D） |
| --- | --- | --- | --- | --- |
| 1 | 本地兑换发弹量 | 本次买 N 发 | `当前累计 + N`，上限 1000（2027 表 5-7） | bit 0-10 ≥ 目标值 |
| 2 | 确认免费复活 | 待复活 | 1（完成后发 0） | 血量从 0 变正 |
| 3 | 兑换立即复活 | 待复活且可兑换 | 1（完成后发 0） | 血量从 0 变正 |
| 4 | 远程兑换发弹量 | 本次 1 次 | `当前次数 + 1` | bit 11-14 ≥ 目标值 |
| 5 | 远程兑换血量 | 本次 1 次 | `当前次数 + 1` | bit 15-18 ≥ 目标值 |

- 发弹量 / 远程次数必须**单调递增**；远程次数协议要求每次只能 +1。
- 最小兑换单位（2027 表 5-8 / 5-9）：17mm 增益点 **10 发**，远程 **100 发**。
  本地兑换的增量必须是 10 的整数倍；远程的 `value` 是次数，`+1` 就代表 100 发。
- 复活是**电平位**：待复活期间持续置 1；复活完成或放弃后补发 0。
- **所有动作只发一次，不重试**。任务内只等待检测窗口；窗口结束仍未生效就返回失败，由动作层重新决策。
- `Resource`：发送 `kind=1`，目标为 `baseline + amount`；5 s 内等 `info1.ammo_exchanged_local >= target`。
- `Heal` 的基地免费发弹没有 `ActionRequest`：存活在补给区即自动领取；`Heal` 只做 5 s 到账确认。
- 旧 `ActionRequest.retry_interval` / `max_attempts`、`ActionDispatcher` 重发状态机、resource BT 的
  `retry_ms` / `max_attempts` 端口、配置项及相关测试全部移除。

## 7. 生效校验链路

```text
决策 --DecisionCommand(request_id=N, task_state, value=目标累计值)--> auto-aim --0x02--> MCU
MCU --0x0120--> 裁判
决策 <--0x020D(sentry_info_1 / self_hp)-- 裁判
```

- MCU 不需要回执，也不需要判断成功；它只维护 0x0120 的字段状态并按节拍转发。
- 决策侧只做「发送一次 → 5 s 检测窗口 → 成功 / 失败」；不再维护重试状态机。
- 累计目标在同一局内单调不减；离开「比赛中」视为新一局，重置下界。

## 8. 实现现状

| 环节 | 状态 |
| --- | --- |
| 决策侧订阅 4 条上行、发布 `DecisionCommand` | ✅ `RosIoNode` |
| 决策侧单次下发 + 检测窗口 | ✅ `ActionDispatcher` 已改为单次发送；任务层做 5 s 检测 |
| auto-aim 上行（`Publish2DecisionMaking` 发布 4 条消息） | ✅ 已迁到 `sentry_interfaces` |
| auto-aim 下行（`Subscribe2Decision`） | ✅ 已按 10 B 组帧并转发 `task_state` |
| auto-aim 串口字节帧 v1 | ✅ 帧收发已实现（见 `serial_protocol.md`） |
| MCU 侧实现 v1 帧 | ⛔ 待电控 / MCU（当前面向上位机联调） |
| `SentryInfoOffline` 视觉锁定字段接入决策 | ⛔ 待 auto-aim 侧补齐 |
| `TeamInfo.allies[4]` 接入 `WorldState.allies` | ⛔ 待接入（见 [BELIEF.md](BELIEF.md) §7） |

## 9. 未决项

- `GameInfo.event_code_2` 已新增；auto-aim 已转发，MCU 固件待按 33 B 上行实现，未填充时基地控制状态按 0 处理。
- `own_color` 由 `RosIoNode` 参数 `own_color`（red / blue）提供，用于 R/B → own/enemy 映射。
- `DecisionCommand.task_state` 已加入 ROS 消息与 auto-aim；待 `TaskRuntime` 填充并同步 MCU 固件。
- `DecisionOutput.tactical_mode` 已删除，面板 / 回放改用 `task_state`。
- `detect_color` 编码含义（当前不依赖；颜色由 `RosIoNode` 的 `own_color` 参数给出）。
- MCU 合并 0x0120 状态时的姿态位（bit 21-23）由谁维护。
- 串口帧的 `magic` / 发送节拍是否最终确认。
- 是否纳入更多上行字段（金币、能力机关、`sentry_info_3` / 电容 / 热量等）。
