# 使用说明

> 面向使用者与拉车同学的命令速查：怎么构建、怎么跑、有哪些参数。
> 本文只列命令与参数；仿真 / 场景 / 网页面板的操作细节见 [SIMULATION.md](SIMULATION.md)，
> 架构见 [ARCHITECTURE.md](ARCHITECTURE.md)，信念字段见 [BELIEF.md](BELIEF.md)。

## 1. 前置条件

- Docker；
- 镜像 `sentry_decision_rm27:jazzy`（首次需构建，见下）。

## 2. 快速开始

### 使用 Docker 构建和运行（不依赖 LTS24.04 系统）

```bash
# 构建镜像（首次，含 rosbridge 供网页面板）
docker build -f docker/Dockerfile -t sentry_decision_rm27:jazzy .

# 构建工作区
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy build

# 构建 + 全部测试（容器内）
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy test

# 进入交互容器（后续命令都在容器内；先 source）
# -p 8080:8080 / 9090:9090 仅网页面板需要，映射到宿主浏览器
docker run -it --rm -p 8080:8080 -p 9090:9090 \
  -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh \
  sentry_decision_rm27:jazzy shell
source .docker-build/install/setup.bash

# 脚本一键启动「网页面板 + 裁判仿真 + 决策节点」
tools/demo.sh
```

> 网页面板需要镜像含 `ros-jazzy-rosbridge-suite`；仓库 `docker/Dockerfile` 已包含。
> 也可用 Compose：`docker compose -f docker/compose.yaml run --rm decision-dev test`。
> 构建产物在宿主 `.docker-build/`（`install/` 安装空间、`compile_commands.json` 供 IntelliSense）。

### 实机构建和运行

**构建决策仓库**

```bash
cd ~/pnx_autoaim_sp/sentry_decision_RM27
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install

# 测试（可选）
source install/setup.bash
colcon test && colcon test-result --verbose
```

**构建自瞄仓库**

```bash
cd ~/pnx_autoaim_sp/sentry_decision_RM27
source install/setup.bash
# 自瞄依赖 sentry_interfaces，先 source 决策仓库 overlay

cd ~/pnx_autoaim_sp/auto-aim-new
rm -rf build                      # 如果 CMake 有改动，建议清缓存
cmake -B build                    # 自定义依赖目录时加 -DCMAKE_PREFIX_PATH=...
make -C build -j"$(nproc)"
```

**运行联调**

```bash
# 终端 A：自瞄 + ROS 桥（独占串口 /dev/ttyACM0）
cd ~/pnx_autoaim_sp/auto-aim-new
./launch.sh

# 终端 B：决策
cd ~/pnx_autoaim_sp/sentry_decision_RM27
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run sentry_decision_bringup decision_node
# 若导航用的不是 /aft_mapped_to_init：
#   decision_node --ros-args -p odom_topic:=/odometry -p odom_frame:=odom
```

**查看节点**

```bash
source /opt/ros/jazzy/setup.bash
source ~/pnx_autoaim_sp/sentry_decision_RM27/install/setup.bash
ros2 node list  # 应有 gimbal_decision_bridge / gimbal_decision_command_bridge / sentry_decision_io / sentry_decision
ros2 topic list | grep -E 'sentry|decision|cmd_vel'
```

## 3. 入口速查

| 想做什么 | 命令 |
| --- | --- |
| 一键仿真演示（面板 + 仿真 + 决策） | `tools/demo.sh` |
| 本地最小闭环（无需 ROS / MCU / 导航） | `ros2 run sentry_decision_bringup decision_main` |
| 真实 IO 决策闭环（接 auto-aim / 导航） | `ros2 run sentry_decision_bringup decision_node` |
| 只跑 IO 节点 | `ros2 run sentry_decision_io io_node` |
| 裁判仿真（持续发布世界状态） | `ros2 run sentry_decision_sim referee_sim_node` |
| 跑一个场景 | `ros2 run sentry_decision_sim referee_sim_node --scenario "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/full_match.yaml"` |
| 网页面板（rosbridge :9090 + 页面 :8080） | `ros2 launch sentry_decision_viz viz.launch.py` |
| 离线回放 | `ros2 run sentry_decision_bringup replay_main --bag <bag>` |
| 查看决策状态 | `ros2 topic echo /decision/state`（或 `/decision/world_state`、`/decision/tree_status`） |
| 查询面板状态 | `ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand "{command: 'list_state', args: ''}"` |
| 宿主纯逻辑测试 | `tools/host_core_test.sh` |
| 网页面板纯逻辑单测 | `node src/sentry_decision_viz/web/test/format.test.mjs` |
| 格式检查 | `tools/format.sh --check` |

## 4. 运行入口

### 4.1 decision_main（本地最小闭环）

```bash
ros2 run sentry_decision_bringup decision_main
```

不依赖 ROS / 下位机 / 导航仓库，由 `sentry_decision_sim` 的 dummy 裁判系统与伪导航驱动信念层。
演示行为：启动约 1 秒后血量从 400 掉到 50，行为树由巡逻抢占到撤退，伪导航朝新目标移动，
ACT 日志显示 `mode`、`goal` 与当前位置变化。

行为树源文件位于仓库根目录 `tree/`，构建后安装到 `share/sentry_decision_bringup/tree/`；
`--tree` 默认指向安装后的 `tree/root.xml`。行为树按层组织：
`root.xml`（组合）→ `mission/root.xml`（优先级）→ `mission/nav/*`（任务）+ `skill/*`（技能）。

### 4.2 decision_node（真实 IO 闭环）

```bash
ros2 run sentry_decision_bringup decision_node
```

把 `RosIoNode`（订阅 `/sentry/*` 四条上行与 odom，发布 `/cmd_vel`、
`/sentry/decision_command`）、行为树、`IntentArbiter` 与 `DecisionStatePublisher` 放在同一进程闭环。

- **上游**：依赖 auto-aim 提供 `/sentry/game_info` 等话题；`GameInfo` 与 `SentryInfoOnline`
  都出现过才判 `upstream.valid`。上行字段见 [BELIEF.md](BELIEF.md)。
- **定位**：默认订阅 `/aft_mapped_to_init`，可用 `--ros-args` 覆盖（见 §5.2）。
- **导航**：本节点是 Nav2 `navigate_to_pose` 的 **action client**，**不会启动导航栈**。
  没有导航时启动仍正常，只是 `send_goal` 打印「导航 action server 未就绪」并丢弃目标、odom 判失效。
  需另起导航仓库（或仿真里的 `referee_sim_node`）。
- **下位机**：决策不接触串口；`DecisionCommand` 发给 auto-aim，由 auto-aim 经串口转 MCU。

不接 MCU、只验证决策 ↔ auto-aim 的通信时，用 `referee_sim_node` 扮演上行（见 [SIMULATION.md](SIMULATION.md)），
或让 auto-aim 正常连 MCU。

### 4.3 io_node

```bash
ros2 run sentry_decision_io io_node
ros2 node info /sentry_decision_io
```

只跑 IO 适配器，不跑行为树；用于单独检查话题接线。

### 4.4 replay_main（离线回放）

```bash
ros2 bag record -o match_bag \
  /sentry/game_info /sentry/online_info /sentry/offline_info /sentry/team_info \
  /aft_mapped_to_init

ros2 run sentry_decision_bringup replay_main --bag match_bag
```

读 `/sentry/*`（新格式）或旧标量话题，逐 tick 重跑行为树 / 仲裁 / 安全，以 ACT 日志输出现模式变化与统计。
同一份 bag 两次回放逐 tick 一致（`replay_determinism`）。端到端冒烟：`tools/replay_smoke_test.sh`。

### 4.5 裁判仿真与网页面板

`referee_sim_node`、`viz.launch.py`、场景脚本、面板按钮与场地标定工具的操作与语义见
[SIMULATION.md](SIMULATION.md)。

## 5. 命令参数

### 5.1 decision_main

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--ticks` | `50` | tick 次数 |
| `--rate` | `20.0` | tick 频率（Hz） |
| `--hp-drop` | `1.0` | 仿真中血量掉到 50 的秒数 |
| `--tree` | 安装后的 `tree/root.xml` | 行为树入口 XML 路径 |
| `--config` | 安装后的 `config/profiles.yaml` | 配置入口 YAML 路径 |
| `--plugin` | 空 | 节点插件 `.so` 路径；为空时按 `tree_manifest.yaml` 注册 |

```bash
ros2 run sentry_decision_bringup decision_main --ticks 500 --rate 20
```

### 5.2 decision_node

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--rate` | `20.0` | tick 频率（Hz），取值 `(0, 1000]` |
| `--ticks` | `0` | 运行 tick 数，`0` 表示一直运行 |
| `--tree` | 安装后的 `tree/root.xml` | 行为树入口 XML 路径 |
| `--config` | 安装后的 `config/profiles.yaml` | 配置入口 YAML 路径 |
| `--plugin` | 空 | 节点插件 `.so` 路径 |
| `--groot2-port` | `0` | Groot2 监听端口，`0` 表示关闭（范围 `[0, 65535]`） |

内部 `RosIoNode` 的 ROS 参数（见 §5.3）可在 `--ros-args` 之后透传：
```bash
ros2 run sentry_decision_bringup decision_node --ros-args -p odom_topic:=/odometry -p odom_frame:=odom
```

### 5.3 io_node

均为 ROS 参数，可用 `--ros-args -p name:=value` 覆盖。

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `map_frame` | `map` | 导航目标坐标系 |
| `game_info_topic` | `/sentry/game_info` | 比赛宏观信息（GameInfo） |
| `online_info_topic` | `/sentry/online_info` | 自身在线状态（SentryInfoOnline） |
| `offline_info_topic` | `/sentry/offline_info` | 自身视觉 / 形态（SentryInfoOffline） |
| `team_info_topic` | `/sentry/team_info` | 队伍信息（TeamInfo） |
| `decision_command_topic` | `/sentry/decision_command` | 决策下行（DecisionCommand） |
| `odom_topic` | `/aft_mapped_to_init` | 里程计（Odometry） |
| `odom_frame` | 空 | 非空时用 TF `map->odom` 把 odom 位姿转 map 系（双仓库设 `odom`） |
| `cmd_vel_topic` | `cmd_vel` | 速度指令（Twist） |
| `navigate_action` | `navigate_to_pose` | 导航 action（NavigateToPose） |

```bash
ros2 run sentry_decision_io io_node --ros-args \
  -p odom_topic:=/sentry/odom -p navigate_action:=/navigate_to_pose
```

### 5.4 referee_sim_node

上行 / 下行话题与 `io_node` 同名同默认；数值参数来自 `config/sim.yaml`，可用 `-p` 逐项覆盖。
完整的命令行参数、服务与场景说明见 [SIMULATION.md](SIMULATION.md#3-裁判仿真-referee_sim_node)。

### 5.5 replay_main

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--bag` | 必填 | rosbag2 路径（含 `/sentry/*` 上行） |
| `--ticks` | `0` | 最多回放 tick 数，`0` 表示回放到数据结束 |
| `--rate` | `20.0` | 回放步长频率（Hz），取值 `(0, 1000]` |
| `--odom-topic` | `/aft_mapped_to_init` | 里程计话题 |
| `--tree` / `--config` | 安装后的路径 | 行为树 / 配置入口 |

## 6. 决策状态话题

`DecisionStatePublisher` 发布决策快照，供可视化与 rosbag：

| 话题 | 类型 | 说明 |
| --- | --- | --- |
| `/decision/state` | `sentry_decision_msgs/DecisionState` | 每 tick 的仲裁输出、冲突与告警 |
| `/decision/world_state` | `sentry_decision_msgs/WorldState` | 信念快照摘要 |
| `/decision/tree_status` | `sentry_decision_msgs/TreeStatus` | 行为树节点状态与 active path（transient local） |

```bash
ros2 bag record /decision/state /decision/world_state /decision/tree_status
```

`decision_node` 已接线以上发布；`decision_main` 无 ROS，不发布。

## 7. 配置

配置采用「单一入口 + 按职责分文件」（详见 [ARCHITECTURE.md](ARCHITECTURE.md) §15）：

```text
config/
├── profiles.yaml           # 入口：map_profile / strategy_profile / pre_match
├── maps/<MAP>.yaml         # 命名点（可选第 4 位区域半径）、frame
├── policies/<POLICY>.yaml  # 阈值、时间窗、兑换步长、安全限幅、超时
└── sim.yaml                # 仿真参数：max_hp / 导航速度 / 阶段时长 / 区域与效果
```

XML 不写数值：坐标用命名点（`point="home"`），阈值用配置 key（`hp_key="nav.retreat_hp"`）。
启动时校验配置与行为树引用的完整性与取值范围，缺失或越界即启动报错并列出缺项，生效配置打印为 ACT 日志。

## 8. 测试

| 范围 | 命令 |
| --- | --- |
| 宿主 core 单测（无需 ROS） | `tools/host_core_test.sh` |
| 容器全量 | `docker/entrypoint.sh test` |
| io 冒烟 | 容器内 `tools/io_smoke_test.sh` |
| 场景 / 回放 / 面板 / 比赛阶段 / 世界结算冒烟 | `tools/scenario_smoke_test.sh`、`tools/replay_smoke_test.sh`、`tools/viz_smoke_test.sh`、`tools/match_smoke_test.sh`、`tools/sim_effects_smoke_test.sh` |
| 网页面板纯逻辑单测 | `node src/sentry_decision_viz/web/test/format.test.mjs` |
| 格式检查 | `tools/format.sh --check` |

## 9. 环境变量

| 变量 | 默认 | 说明 |
| --- | --- | --- |
| `WS_DIR` | `/ws` | 工作区路径 |
| `BUILD_DIR` | `${WS_DIR}/.docker-build` | 构建产物目录 |
| `INSTALL_DIR` | `/ws/.docker-build/install` | io 冒烟测试使用的安装空间 |

## 10. IDE

推荐使用 Dev Container：安装 Dev Containers 扩展后「Reopen in Container」，
头文件与 `compile_commands.json` 均在容器内解析。

## 11. 常见问题

- `decision_main: command not found`：未 source overlay，执行 `source .docker-build/install/setup.bash`。
- `decision_node` 报「导航 action server 未就绪」：没有启动导航栈，决策不会下发目标；接上 Nav2 或仿真即可（见 §4.2）。
- 上游一直是 invalid：auto-aim 未启动或没收到 MCU 串口帧；`referee_sim_node` 可替代上行（见 [SIMULATION.md](SIMULATION.md)）。
- IntelliSense 报头文件找不到：宿主缺少 Jazzy / BT.CPP 头文件，请用 Dev Container 打开，或检查 `.vscode/c_cpp_properties.json`。
- 镜像中没有 `nav2_behavior_tree`：本项目不依赖它，`nav_executor` 直接使用 `nav2_msgs/action/NavigateToPose`。
