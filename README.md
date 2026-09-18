# Sentry Decision RM27

> 哨兵决策模块 —— RoboMaster RM27 赛季，基于 ROS 2 Jazzy 与 BehaviorTree.CPP v4。

## 项目简介

本仓库是 RM27 哨兵机器人的决策模块，负责汇聚裁判系统、感知、导航等外部信息，通过行为树产生决策，并输出导航目标、姿态、资源兑换等指令。

本项目是旧仓库 `sentry_DecisionMaking` 的重构版本，参考 `navi_minco_bit/src/decision` 的分层设计，在 ROS 2 Jazzy + BehaviorTree.CPP v4 上重新组织代码。详细技术设计见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 项目状态

> :construction: 设计阶段：架构文档已完成，代码骨架尚未搭建。

## 运行环境

| 项目 | 说明 |
| --- | --- |
| 目标运行环境 | Ubuntu 24.04 + ROS 2 Jazzy |
| 开发宿主 | 若宿主非 Ubuntu 24.04（例如 26.04），通过 Docker 开发与测试 |
| 构建系统 | colcon + ament_cmake |
| 关键依赖 | BehaviorTree.CPP v4、rclcpp |

## 快速开始

```bash
# 编译
colcon build --symlink-install
source install/setup.bash
```

> 容器化开发环境与启动方式待确定后补充。

## 目录结构（规划）

```text
sentry_decision_RM27/
├── docs/    # 项目文档
└── src/     # ROS 2 源码包（待创建）
```

> 目录结构随架构确定后更新。

## 文档索引

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)：技术架构设计（单一事实来源）
- [docs/AGENT.md](docs/AGENT.md)：开发者与 AI 协作规范
- [docs/NOTE.md](docs/NOTE.md)：开发临时笔记
- [docs/CHANGELOG.md](docs/CHANGELOG.md)：变更日志

## 贡献

提交规范与开发流程见 [docs/AGENT.md](docs/AGENT.md)。

## 许可证

待定。