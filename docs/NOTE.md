# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P2 策略迁移准备）

- P1 已完成（本地范围）：裁判协议解码、`WorldState` 契约对齐、`sentry_decision_msgs` + `DecisionStatePublisher`、core 回放 `ReplaySource`、rosbag 读取、本地仿真 `sentry_decision_sim`。
- 回归：core 回放确定性单测 + 树级 `replay_determinism`（同一份回放两次输出逐 tick 一致）。
- 暂缓：真实下位机通信包 io 接线（包定义待定，本地开发用仿真）；旧 rosbag 字段一致性对比（暂无样本）。
- 已移除：`sentry_info_3`（姿态剩余强化时间）解码，疑似临时规则，待协议明确。
- 已确认（暂缓落地）：下位机通信包动作分 `kOneShot` / `kPolled`，配置时显式选择，`kPolled` 带轮询间隔，见 `docs/ARCHITECTURE.md` §4.1。
- 已同步：stance（姿态）为旧规则已废弃，删除相关类型 / 逻辑 / 消息 / 文档；行为树迁移到仓库根目录 `tree/`。
- P2 重点：行为树目录与插件清单（`tree/`）、战术 / 技能模块、`nav_policy` + `nav_executor`、`intervention` 模块。
- 待决策：`sentry_decision_msgs` 的 action / service 字段；回放差异报告格式。
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
