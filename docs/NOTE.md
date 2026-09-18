# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P1 信念与回放）

- 进行中：完善 `WorldState` 契约与裁判协议解码。
- 待做：`replay` 适配器与确定性回放、`DecisionState` 发布与 rosbag 记录、bringup 切换到真实 io。
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
