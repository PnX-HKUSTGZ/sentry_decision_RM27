# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P1 信念与回放）

- 已完成：裁判协议位段解码 `referee_protocol`（纯函数 + 宿主单测）。
- 已完成：`WorldState` 契约按参考 `ros_interfaces` 字段对齐（GameInfo / TeamInformation / RadarInfo / SentryInfoOnline / SentryInfoOffline），`SelfState` 预留 IMU 四元数 `SelfState.imu`。
- 已移除：`sentry_info_3`（姿态剩余强化时间）解码，疑似临时规则，待协议明确。
- 已确认（暂缓落地）：下位机通信包定义位置未定，不在 core 落地 `UplinkFrame` / `DownlinkFrame`；动作分 `kOneShot` / `kPolled`，配置时显式选择，`kPolled` 带轮询间隔，见 `docs/ARCHITECTURE.md` §4.1。
- 已完成：新增 `sentry_decision_msgs`（`DecisionState` / `WorldState` / `DecisionOutput`）与 io 的 `DecisionStatePublisher`（发布 `/decision/state`、`/decision/world_state`），含纯转换与容器单测。
- 已完成：core 确定性回放 `ReplaySource` / `ReplayData`（固定步长、按时间取最近记录、时间戳 = epoch + at），含宿主单测。
- 已完成：io 的 `load_replay_data` 从 rosbag2 读取旧话题并填充 `ReplayData`，含往返测试。
- 进行中：回放回归（新旧输出逐 tick 对比）；io 接线。
- 待决策：对外消息包 `sentry_decision_msgs` 的字段范围；回放文件格式（rosbag2 + 时间轴抽象）。
- 阶段、顺序与验收见 `docs/ROADMAP.md`；编码与文档规范见 `docs/CONVENTIONS.md`。

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
