# Changelog

> 记录会影响用户、开发者或贡献者体验的重要变更。面向人类阅读，应随版本发布更新。
> 格式参考 [Keep a Changelog](https://keepachangelog.com/)。
> [备忘] 变更类型：Added（新增）、Changed（变更）、Deprecated（弃用）、Removed（移除）、Fixed（修复）、Security（安全）。

> [注意] 本文档是项目历史的单一事实来源，应随项目演进保持更新。
> [注意] 本文档不是所有变更的完整清单；只记录重要变更并保持简洁易读。完整变更见版本控制系统（如 Git）历史。

## [Unreleased]

### Added

- 初始化项目文档（README、AGENT、ARCHITECTURE、NOTE、CHANGELOG、.gitignore）
- 完成架构设计初版：分层、抢占与仲裁、人工干预模块
- `sentry_decision_core`：数据契约与字段级 `IntentArbiter`，含宿主单元测试
- 新增 `docs/ROADMAP.md` 开发路线图与 `docs/CONVENTIONS.md` 代码 / 文档规范
- `sentry_decision_core`：分级日志器（完整 / 简短双通道），含宿主单元测试
- `sentry_decision_core`：IO 端口抽象与信念层 `WorldModel`（含超时降级），含宿主与容器测试
- `sentry_decision_core`：新增 `DecisionContext`（世界状态 + 每 tick 意图缓冲）
- `sentry_decision_nodes`：最小行为树插件（`CheckLowHp` / `EmitTacticalMode` / `EmitNavGoal`）与节点注释模板
- `sentry_decision_bringup`：`main` 最小闭环（WorldModel → 行为树 → IntentArbiter）、演示树与 launch
- 新增 `docker/`（Dockerfile、entrypoint、compose）、`.dockerignore` 与 CI
- `sentry_decision_io`：ROS IO 适配器（订阅 / 发布 / action / service），含容器冒烟测试
- 新增 `.vscode`（IntelliSense 与推荐扩展）与 `.devcontainer` 配置
- Docker 镜像新增与宿主同 UID 的 `dev` 用户；构建产出合并的 `compile_commands.json`

### Changed

### Fixed
