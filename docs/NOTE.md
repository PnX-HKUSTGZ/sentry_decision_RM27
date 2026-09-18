# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前 sprint（P0 骨架）

- 下一步：bringup 最小闭环（`main` + 最小可 tick 树），随后 CI。
- 阶段、顺序与验收见 `docs/ROADMAP.md`；编码与文档规范见 `docs/CONVENTIONS.md`。

## 本 sprint 已完成

- 仓库文档与架构设计（`docs/ARCHITECTURE.md`）。
- `sentry_decision_core`：数据契约与字段级 `IntentArbiter`。
- `sentry_decision_core`：分级日志器（完整 / 简短双通道）。
- `sentry_decision_core`：IO 端口抽象（`RefereeSource` / `OdometrySource` / `NavigationSink` / `ChassisSink`）与信念层 `WorldModel`。
- `docker/`：Dockerfile、entrypoint、compose、`.dockerignore`；镜像 `sentry_decision_rm27:jazzy` 构建通过，BT.CPP 4.10.0（旧镜像为 4.9.0，后续需锁定版本）。
- 验证：宿主 `tools/host_core_test.sh` 全部通过；Jazzy 容器内 colcon 测试 3 tests / 0 failures。

## 注意事项

- 当前无法上实车，一律以本地 PC 容器验证为准。
- 不自动执行 Git 操作；不自动执行系统级操作。
- 文档优先（docs-first），设计变更先改文档再改代码。
- 未确定项集中记录在 `docs/ARCHITECTURE.md` 第 16 节。
