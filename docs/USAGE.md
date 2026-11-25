# 使用说明

> 命令速查；仿真 / 场景 / 网页面板的操作细节见 [SIMULATION.md](SIMULATION.md)，
> 架构见 [ARCHITECTURE.md](ARCHITECTURE.md)，信念字段见 [BELIEF.md](BELIEF.md)。

## 1. 前置条件

- Docker；
- 镜像 `sentry_decision_rm27:jazzy`（首次需构建，见下）。

## 2. 快速开始

### 使用 Docker 构建和运行（不依赖 LTS24.04 系统）

初次运行需构建镜像和工作区，后续可直接运行仿真。

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
```

#### 本地仿真脚本

```bash
# 脚本一键启动「裁判仿真 + 决策节点 + 网页面板」，面板默认开启
tools/demo.sh [--build] # 首次运行需加 --build，后续可直接运行

# 默认已是 RMUC27；需要显式指定时：
tools/demo.sh --config config/profiles_rmuc27.yaml

# 无头模式：不启动网页面板，也不打开导航 RViz/Gazebo
tools/demo.sh --headless

# 需要导航侧界面时按需打开
#   --gazebo        导航侧 Gazebo 客户端窗口
#   --nav-viz       导航 RViz（等价旧写法 --nav-rviz）
#   --nav-gimbal-panel  导航仿真里的云台控制面板
#   --no-viz / --nav-silent  兼容旧写法，只关闭对应窗口

# 双仓库联合仿真（额外起导航容器，导航窗口默认关闭）
# 默认真值定位 + legacy Nav2：自动出生，RViz 直接有地图/机器人，不依赖初始位姿
tools/demo.sh --with-nav [--nav-viz] [--gazebo]
# 真实定位链路（Point-LIO + MINCO + 全局重定位）；当前分支需要 RViz 选出生点
tools/demo.sh --with-nav --nav-real --nav-viz
xhost +SI:localuser:root # 若遇到 RViz/Gazebo 无法显示，执行此命令允许容器访问宿主 X11

> 导航镜像必须包含 RMUC2027 世界；旧镜像会报 `Unknown world 'RMUC2027'`。
> 更新导航仓库后需要从该仓库重新构建 `rm27_nav:jazzy`，不能继续用旧镜像。
```

#### 启动导航仓库

```bash
docker run -it --rm --name nav_dev \
  --network host \
  -e DISPLAY="$DISPLAY" \
  -e XAUTHORITY=/tmp/.Xauthority \
  -v "${XAUTHORITY:-$HOME/.Xauthority}":/tmp/.Xauthority:ro \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  -v "$PWD/src":/ws/src \
  -v "$PWD/src/pnx-launch/launch_bringup/pcd/simulation":/pcd:ro \
  rm27_nav:jazzy bash
```

进入后：

```bash
source /ws/install/setup.bash

ros2 launch rm_27_stimulation sim_with_nav.launch.py \
  world:=RMUC2027 \
  nav_world:=auto \
  use_ground_truth_odom:=false \
  navigation_mode:=minco \
  enable_legacy_terrain:=auto \
  gui:=true \
  use_rviz:=true
```

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

# 终端 B：2027 Task 层（新）
cd ~/pnx_autoaim_sp/sentry_decision_RM27
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run sentry_decision_bringup task_node

# 默认配置已是 RMUC27；显式指定时：
ros2 run sentry_decision_bringup task_node --config config/profiles_rmuc27.yaml
# 或
ros2 launch sentry_decision_bringup task.launch.py \
  config:=$HOME/pnx_autoaim_sp/sentry_decision_RM27/config/profiles_rmuc27.yaml

# 若导航用的不是 /aft_mapped_to_init：
#   task_node --ros-args -p odom_topic:=/odometry -p odom_frame:=odom
# 蓝方：
#   task_node --ros-args -p own_color:=blue
```

**查看节点**

```bash
source /opt/ros/jazzy/setup.bash
source ~/pnx_autoaim_sp/sentry_decision_RM27/install/setup.bash
ros2 node list  # 应有 gimbal_decision_bridge / gimbal_decision_command_bridge / sentry_decision_io / sentry_task_decision
ros2 topic list | grep -E 'sentry|decision|cmd_vel'
```



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

- `task_node: command not found`：未 source overlay，执行 `source .docker-build/install/setup.bash`。
- `task_node` 报「导航 action server 未就绪」：没有启动导航栈，决策不会下发目标；接上 Nav2 或仿真即可（见 §4.2）。
- 上游一直是 invalid：auto-aim 未启动或没收到 MCU 串口帧；`referee_sim_node` 可替代上行（见 [SIMULATION.md](SIMULATION.md)）。
- IntelliSense 报头文件找不到：宿主缺少 Jazzy / BT.CPP 头文件，请用 Dev Container 打开，或检查 `.vscode/c_cpp_properties.json`。
- 镜像中没有 `nav2_behavior_tree`：本项目不依赖它，`nav_executor` 直接使用 `nav2_msgs/action/NavigateToPose`。
