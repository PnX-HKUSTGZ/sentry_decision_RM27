# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P0 骨架）

- 进行中：CI 格式检查；后续补 `io` 的 sim / replay 实现。
- 已确定：BT tick 频率默认 20 Hz（可配置）。
- 阶段、顺序与验收见 `docs/ROADMAP.md`；编码与文档规范见 `docs/CONVENTIONS.md`。

## 本 sprint 已完成

- 仓库文档与架构设计（`docs/ARCHITECTURE.md`）。
- `sentry_decision_core`：数据契约、字段级 `IntentArbiter`、分级日志器、IO 抽象、信念层 `WorldModel`、`DecisionContext`。
- `sentry_decision_io`：单一 `RosIoNode` 实现四个 IO 端口接口，含订阅 / 发布 / action / service 客户端。
- `sentry_decision_nodes`：`CheckLowHp` / `EmitTacticalMode` / `EmitNavGoal` 插件与节点注释模板。
- `sentry_decision_bringup`：`main` 最小闭环（WorldModel → 行为树 → IntentArbiter）、演示树与 launch。
- `docker/` 与 `.github/workflows/ci.yml`；镜像 `sentry_decision_rm27:jazzy` 构建通过（BT.CPP 4.10.0，含与宿主同 UID 的 `dev` 用户）。
- IDE：`.vscode`（IntelliSense / 推荐扩展）与 `.devcontainer`（在容器内解析头文件）；构建产出合并的 `compile_commands.json`。
- 验证：宿主 core 全部通过；容器内 4 包构建、5 tests / 0 failures；实跑展示 `patrol → retreat` 抢占；`io` 冒烟测试通过。

## 注意事项

- 当前无法上实车，一律以本地 PC 容器验证为准。
- 不自动执行 Git 操作；不自动执行系统级操作。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 未确定项集中记录在 `docs/ARCHITECTURE.md` 第 16 节。
