# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P0 骨架）

- 下一步：IO 接口抽象与 bringup 最小闭环。
- 阶段、顺序与验收见 `docs/ROADMAP.md`；编码与文档规范见 `docs/CONVENTIONS.md`。

## 本 sprint 已完成

- 仓库文档与架构设计（`docs/ARCHITECTURE.md`）。
- `sentry_decision_core`：数据契约与字段级 `IntentArbiter`。
- `sentry_decision_core`：分级日志器（完整 / 简短双通道）。
- 宿主单测 `tools/host_core_test.sh`（无 ROS，g++ 直接编译运行），arbiter + logging 全部通过。

## 注意事项

- 不自动执行 Git 操作；不自动执行系统级操作。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 未确定项集中记录在 `docs/ARCHITECTURE.md` 第 16 节。
