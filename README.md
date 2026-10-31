# Sentry Decision RM27

> 哨兵决策模块 —— RoboMaster RM27 赛季，基于 ROS 2 Jazzy 与 BehaviorTree.CPP v4。

## 项目简介

本仓库是 RM27 哨兵机器人的决策模块，负责汇聚裁判系统、感知、导航等外部信息，通过行为树产生决策，并输出导航目标、姿态、资源兑换等指令。

本项目是旧仓库 `sentry_DecisionMaking` 的重构版本，参考 `navi_minco_bit/src/decision` 的分层设计，在 ROS 2 Jazzy + BehaviorTree.CPP v4 上重新组织代码。详细技术设计见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 项目状态

> :white_check_mark: **v0.5.2**：决策 ↔ auto-aim 的 ROS 上位机链路（`sentry_interfaces`）已落地；取消执行回执，改由决策侧用裁判上行校验生效并重试；动作的重试策略由行为树随 `ActionRequest` 传入。
> 信念层完成一致性改名（`RefereeState` → `UpstreamState`），并移除未启用的雷达 / 变形字段。
>
> 下一步（v0.6.0 / P4）：实车与导航仓库联合跑通，MCU 侧实现协议 v1。完整历史见 [docs/CHANGELOG.md](docs/CHANGELOG.md)，当前进度见 [docs/NOTE.md](docs/NOTE.md)。

## 运行环境

| 项目 | 说明 |
| --- | --- |
| 目标运行环境 | Ubuntu 24.04 + ROS 2 Jazzy |
| 开发宿主 | 若宿主非 Ubuntu 24.04（例如 26.04），通过 Docker 开发与测试 |
| 构建系统 | colcon + ament_cmake |
| 关键依赖 | BehaviorTree.CPP v4、rclcpp、rosbridge（网页面板）、yaml-cpp |

## 快速开始

```bash
# 构建镜像（首次）
docker build -f docker/Dockerfile -t sentry_decision_rm27:jazzy .

# 构建工作区
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh sentry_decision_rm27:jazzy build

# 构建并测试
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh sentry_decision_rm27:jazzy test

# 运行最小决策闭环（无需 ROS）
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /bin/bash sentry_decision_rm27:jazzy \
  -lc "source /ws/.docker-build/install/setup.bash && ros2 run sentry_decision_bringup decision_main"

# 一键运行本地仿真（裁判 + 行为树 + 网页面板）
tools/demo.sh

# 双仓库联合仿真（额外起真实导航）
tools/demo.sh --with-nav [--nav-silent] # --nav-silent 关闭导航 RViz
```

完整命令与参数见 [docs/USAGE.md](docs/USAGE.md)；仿真、场景与面板操作见 [docs/SIMULATION.md](docs/SIMULATION.md)。

## 目录结构

```text
sentry_decision_RM27/
├── docs/                      # 项目文档
├── docker/                    # 开发 / 测试容器
├── tools/                     # 宿主测试脚本
├── tree/                      # 行为树 XML（tree_manifest.yaml、root.xml 等）
├── config/                    # 外置参数（profiles / maps / policies / sim）
├── .github/workflows/         # CI
└── src/
    ├── sentry_decision_core/    # 数据契约、信念、仲裁、日志
    ├── sentry_decision_msgs/    # 对外消息：DecisionState / WorldState / TreeStatus
    ├── sentry_interfaces/       # 与 auto-aim 的上位机接口契约
    ├── sentry_decision_io/      # ROS IO 适配器与决策状态发布
    ├── sentry_decision_sim/     # 本地仿真：裁判仿真、场景脚本与伪导航
    ├── sentry_decision_viz/     # 观测：树状态、Groot2 桥与 rosbridge 网页面板
    ├── sentry_decision_nodes/   # 行为树节点插件
    └── sentry_decision_bringup/ # main、launch（行为树见根目录 tree/）
```

## 文档索引

面向不同读者分为三层：

**使用与操作**（面向使用者 / 拉车同学）

- [docs/USAGE.md](docs/USAGE.md)：命令速查与各入口参数
- [docs/SIMULATION.md](docs/SIMULATION.md)：仿真、场景脚本、网页面板与场地标定

**设计与契约**（面向开发者）

- [docs/OVERVIEW.md](docs/OVERVIEW.md)：项目概览与设计思路（先读这个）
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)：技术架构设计（单一事实来源）
- [docs/BELIEF.md](docs/BELIEF.md)：信念层（`WorldState`）字段字典与有效性判定
- [docs/INTERFACES.md](docs/INTERFACES.md)：上位机接口契约（auto-aim ↔ 决策）

**流程与记录**（面向协作者）

- [docs/ROADMAP.md](docs/ROADMAP.md)：开发路线图（阶段、顺序、验收）
- [docs/CONVENTIONS.md](docs/CONVENTIONS.md)：代码与文档规范
- [docs/AGENT.md](docs/AGENT.md)：开发者与 AI 协作规范
- [docs/NOTE.md](docs/NOTE.md)：当前 sprint 临时笔记
- [docs/CHANGELOG.md](docs/CHANGELOG.md)：变更日志

## 贡献

提交规范与开发流程见 [docs/AGENT.md](docs/AGENT.md)。

## 许可证

待定。
