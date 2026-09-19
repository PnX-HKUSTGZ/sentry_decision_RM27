# 使用说明

> 面向使用者的命令速查与参数说明。架构见 `docs/ARCHITECTURE.md`，开发规范见 `docs/CONVENTIONS.md`。

## 1. 前置条件

- Docker；
- 镜像 `sentry_decision_rm27:jazzy`（首次需构建）。

## 2. 构建镜像

```bash
docker build -f docker/Dockerfile -t sentry_decision_rm27:jazzy .
```

## 3. 工作区构建 / 测试

容器入口 `docker/entrypoint.sh` 支持 `build` / `test` / `shell`。

```bash
# 构建（生成 .docker-build/install 与 compile_commands.json）
docker run --rm -v "$PWD":/ws -w /ws \
  --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy build

# 构建并运行测试
docker run --rm -v "$PWD":/ws -w /ws \
  --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy test

# 交互 shell（ROS 已 source，overlay 需自行 source）
docker run -it --rm -v "$PWD":/ws -w /ws \
  --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy shell
```

也可用 Compose：

```bash
docker compose -f docker/compose.yaml run --rm decision-dev build
docker compose -f docker/compose.yaml run --rm decision-dev test
docker compose -f docker/compose.yaml run --rm decision-dev shell
```

构建产物在宿主 `.docker-build/`：

- `.docker-build/install/`：colcon 安装空间；
- `.docker-build/compile_commands.json`：合并后的编译数据库（IntelliSense 使用）。

## 4. 运行

进入交互容器并 source overlay：

```bash
docker run -it --rm -v "$PWD":/ws -w /ws \
  --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy shell

# 容器内：
source .docker-build/install/setup.bash
```

### 4.1 最小决策闭环

```bash
ros2 run sentry_decision_bringup decision_main
```

本地仿真：由 `sentry_decision_sim` 的 dummy 裁判系统与伪导航驱动信念层，无需下位机通信包与 navigation 仓库。
演示行为：启动约 1 秒后血量从 400 掉到 50，行为树由巡逻抢占到撤退，伪导航朝新目标移动，ACT 日志显示 `mode`、
`goal` 与当前位置变化。

### 4.2 IO 节点

```bash
ros2 run sentry_decision_io io_node
ros2 node info /sentry_decision_io
```

### 4.3 决策状态话题

`sentry_decision_io::DecisionStatePublisher` 负责发布决策快照（供可视化与 rosbag）：

| 话题 | 类型 | 说明 |
| --- | --- | --- |
| `/decision/state` | `sentry_decision_msgs/DecisionState` | 每 tick 的仲裁输出、冲突与告警 |
| `/decision/world_state` | `sentry_decision_msgs/WorldState` | 信念快照摘要 |

```bash
ros2 bag record /decision/state /decision/world_state
```

该发布类待决策节点接线后生效（P1 后续）。

## 5. 命令参数

### 5.1 decision_main

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--ticks` | `50` | tick 次数 |
| `--rate` | `20.0` | tick 频率（Hz） |
| `--hp-drop` | `1.0` | 仿真中血量掉到 50 的秒数 |
| `--tree` | 安装后的 `demo_tree.xml` | 行为树 XML 路径 |
| `--plugin` | 空 | 节点插件 `.so` 路径；为空时使用链接注册 |

```bash
ros2 run sentry_decision_bringup decision_main --ticks 500 --rate 20
ros2 run sentry_decision_bringup decision_main --plugin /path/to/libsentry_decision_nodes.so
```

### 5.2 io_node 参数

均为 ROS 参数，可用 `--ros-args -p name:=value` 覆盖。

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `map_frame` | `map` | 导航目标坐标系 |
| `health_topic` | `/ifhealth` | 己方血量（UInt16） |
| `ammo_topic` | `/remain_ammo` | 剩余弹量（UInt16） |
| `base_health_topic` | `/our_base_health` | 己方基地血量（UInt16） |
| `our_outpost_topic` | `/our_outpost_health` | 己方前哨血量（UInt16） |
| `enemy_outpost_topic` | `/enemy_outpost_health` | 敌方前哨血量（UInt16） |
| `can_rebuild_topic` | `/can_rebuild_outpost` | 是否可重建前哨（Bool） |
| `odom_topic` | `/odom` | 里程计（Odometry） |
| `cmd_vel_topic` | `cmd_vel` | 速度指令（Twist） |
| `navigate_action` | `navigate_to_pose` | 导航 action（NavigateToPose） |
| `set_bool_service` | `/set_bool` | 底盘标志服务（SetBool） |

```bash
ros2 run sentry_decision_io io_node --ros-args \
  -p odom_topic:=/sentry/odom \
  -p navigate_action:=/navigate_to_pose
```

## 6. 测试

| 范围 | 命令 |
| --- | --- |
| 宿主 core 单测（无需 ROS） | `tools/host_core_test.sh` |
| 容器全量 | `docker/entrypoint.sh test` |
| io 冒烟 | 容器内 `tools/io_smoke_test.sh` |
| 格式检查 | `tools/format.sh --check` |

## 7. 环境变量

| 变量 | 默认 | 说明 |
| --- | --- | --- |
| `WS_DIR` | `/ws` | 工作区路径 |
| `BUILD_DIR` | `${WS_DIR}/.docker-build` | 构建产物目录 |
| `INSTALL_DIR` | `/ws/.docker-build/install` | io 冒烟测试使用的安装空间 |

## 8. IDE

推荐使用 Dev Container：安装 Dev Containers 扩展后「Reopen in Container」，头文件与 `compile_commands.json` 均在容器内解析。

## 9. 常见问题

- `decision_main: command not found`：未 source overlay，执行 `source .docker-build/install/setup.bash`。
- IntelliSense 报头文件找不到：宿主缺少 Jazzy / BT.CPP 头文件，请用 Dev Container 打开，或检查 `.vscode/c_cpp_properties.json`。
- 镜像中没有 `nav2_behavior_tree`：本项目不依赖它，`nav_executor` 直接使用 `nav2_msgs/action/NavigateToPose`。
