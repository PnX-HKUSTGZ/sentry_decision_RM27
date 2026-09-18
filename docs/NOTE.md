# 开发笔记

> 短期临时文档：只记录当前进度、TODO 与注意事项，不保留历史。历史变更见 `docs/CHANGELOG.md`。
> 仅作为开发草稿纸，不是永久文档，也不属于项目正式文档。

## 当前阶段

架构讨论完成，`docs/ARCHITECTURE.md` 已产出初版；代码骨架尚未搭建。

## 已对齐的架构决策

1. 流水线为「信念 + 意图（战术 / 战略 → 任务 → 技能）+ 指令」四段。
2. 高层战术可不用行为树，用状态机 / 效用函数，并封装为 `StrategicPolicy` 接口，未来可替换为学习模型。
3. 抢占主要依赖 `ReactiveFallback` / `ReactiveSequence`；异步节点必须实现可中断（`halt` 取消）契约。
4. 仲裁为指令层的显式组件 `IntentArbiter`：字段级合并 + 集中优先级表 + owner 白名单 + lease；
   `SafetySupervisor` 占最高优先级带并在仲裁后 clamp。
5. 导航拆为 `nav_policy`（意图层 / nodes 插件）与 `nav_executor`（执行层 / io 适配器），全系统单一 `nav_goal`。
6. 人工干预封装为独立模块 `intervention`，通过世界状态注入、意图注入、模块开关三种方式介入，
   走强类型 action + 通用调试 service，支持网页面板与场景脚本，全程可回放，且不能绕过安全。
7. 包粒度：先落地 `core / io / nodes / bringup`，`msgs / viz / sim / test` 按需再拆。
8. 回放优先：凡决策输入、输出、人工干预都必须可记录、可确定性重放。

## TODO

- [ ] 编写 BT 节点注释模板的 CI 检查脚本与节点目录生成器。
- [ ] 验证 `ros-jazzy-nav2-behavior-tree` 在目标镜像中的可用性。
- [ ] 确定 BT tick 频率并保证文档与实现一致。
- [ ] 冻结对外接口清单（topic / service / action 名字）。
- [ ] 确定可视化选型与页面技术栈。
- [ ] 设计回放文件格式与录制范围。
- [ ] 锁定 Jazzy 镜像中的 BT.CPP 版本并复验抢占语义。
- [ ] 搭建 P0 骨架（core 数据结构、日志器、Docker Compose、CI）。

## 注意事项

- 不自动执行 Git 操作；不自动执行系统级操作。
- 文档优先（docs-first），设计变更先改文档再改代码。
