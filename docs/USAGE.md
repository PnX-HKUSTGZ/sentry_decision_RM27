# 使用说明

> 面向使用者的命令速查与参数说明。架构见 `docs/ARCHITECTURE.md`，开发规范见 `docs/CONVENTIONS.md`。

## TL;DR

```bash
# 构建镜像（含 rosbridge，供网页面板）
docker build -f docker/Dockerfile -t sentry_decision_rm27:jazzy .

# 构建 + 全部测试（容器内）
docker run --rm -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh sentry_decision_rm27:jazzy test

# 进入交互容器（后续命令都在容器内；先 source）
# -p 8080:8080 / 9090:9090 仅网页面板需要，映射到宿主浏览器
docker run -it --rm -p 8080:8080 -p 9090:9090 \
  -v "$PWD":/ws -w /ws --entrypoint /ws/docker/entrypoint.sh sentry_decision_rm27:jazzy shell
source .docker-build/install/setup.bash

# 一键起「网页面板 + 裁判仿真 + 决策节点」（宿主直接运行；Ctrl+C 停止全部）
tools/demo.sh
```

容器内常用命令：

| 想做什么 | 命令 |
| --- | --- |
| 一键仿真演示（面板 + 仿真 + 决策，见 §4.0） | `tools/demo.sh`（宿主或容器内均可） |
| 本地最小闭环（无需 MCU / 导航） | `ros2 run sentry_decision_bringup decision_main` |
| 真实 IO 决策闭环 | `ros2 run sentry_decision_bringup decision_node` |
| 裁判仿真（持续发布世界状态） | `ros2 run sentry_decision_sim referee_sim_node` |
| 跑一个场景 | `ros2 run sentry_decision_sim referee_sim_node --scenario "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/full_match.yaml"` |
| 面板演示（稳定非零世界，不退出） | `ros2 run sentry_decision_sim referee_sim_node --scenario "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml" --hold` |
| 网页面板（rosbridge :9090 + 页面 :8080） | `ros2 launch sentry_decision_viz viz.launch.py` |
| 离线回放 | `ros2 run sentry_decision_bringup replay_main --bag <bag>` |
| 查看决策状态 | `ros2 topic echo /decision/state`（或 `/decision/tree_status`、`/decision/world_state`） |
| 查询干预 / 模块状态 | `ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand "{command: 'list_state', args: ''}"` |
| 宿主纯逻辑测试 | `tools/host_core_test.sh` |
| 网页面板纯逻辑单测 | `node src/sentry_decision_viz/web/test/format.test.mjs` |
| 格式检查 | `tools/format.sh --check` |

> 网页面板需要镜像含 `ros-jazzy-rosbridge-suite`；仓库 `docker/Dockerfile` 已包含，重建镜像即可。
> 各功能的完整说明见下文对应章节。

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

### 4.0 一键启动仿真演示（推荐）

不想每次开三个终端，用一条命令拉起「网页面板 + 裁判仿真 + 决策节点」：

```bash
tools/demo.sh            # 宿主直接运行；自动起容器并映射 8080/9090
```

- 三个进程的 stdout 汇到当前终端，分别加 `[viz]` / `[sim]` / `[decision]` 前缀；
  同时各写一份到 `.docker-build/demo_logs/<name>.log`，可另开终端 `tail -f` 它们。
- `Ctrl+C` 停止全部（脚本会终止整组进程）。
- 常用变体：

  | 命令 | 作用 |
  | --- | --- |
  | `tools/demo.sh --no-viz` | 只起裁判仿真 + 决策（不起 rosbridge / 静态页） |
  | `tools/demo.sh --build` | 先 `colcon build` 再启动 |
  | `tools/demo.sh --scenario <容器内路径>` | 指定场景（默认 `demo.yaml`） |
  | `tools/demo.sh --rate 20` | 仿真频率 |

- 已经在容器里时同样可用：`tools/demo.sh` 会自动识别容器环境；也可用
  `docker/entrypoint.sh demo`。
- 宿主运行需镜像已构建、8080/9090 空闲；面板地址 `http://localhost:8080`。

### 4.1 最小决策闭环

```bash
ros2 run sentry_decision_bringup decision_main
```

本地仿真：由 `sentry_decision_sim` 的 dummy 裁判系统与伪导航驱动信念层，无需下位机通信包与 navigation 仓库。
演示行为：启动约 1 秒后血量从 400 掉到 50，行为树由巡逻抢占到撤退，伪导航朝新目标移动，ACT 日志显示 `mode`、
`goal` 与当前位置变化。

行为树源文件位于仓库根目录 `tree/`，构建后安装到 `share/sentry_decision_bringup/tree/`；`--tree` 默认指向安装后 `tree/root.xml`。
行为树按层组织：`root.xml`（组合）→ `mission/root.xml`（优先级）→ `mission/nav/*`（任务）+ `skill/*`（技能）；
点位与阈值在 `config/`，XML 只引用命名点与配置 key。

### 4.2 真实 IO 决策闭环

```bash
ros2 run sentry_decision_bringup decision_node
```

`decision_node` 把 `RosIoNode`（订阅 `/sentry/*`、`/sentry/decision_ack`，发布 `/cmd_vel` 与
`/sentry/decision_command`）、行为树、`IntentArbiter` 与 `DecisionStatePublisher` 放在同一进程闭环。
依赖 auto-aim 提供 `/sentry/game_info` 等话题、定位提供 `/aft_mapped_to_init`；无导航 action server 时告警但不退出。

### 4.3 IO 节点

```bash
ros2 run sentry_decision_io io_node
ros2 node info /sentry_decision_io
```

### 4.4 决策状态话题

`sentry_decision_io::DecisionStatePublisher` 负责发布决策快照（供可视化与 rosbag）：

| 话题 | 类型 | 说明 |
| --- | --- | --- |
| `/decision/state` | `sentry_decision_msgs/DecisionState` | 每 tick 的仲裁输出、冲突与告警 |
| `/decision/world_state` | `sentry_decision_msgs/WorldState` | 信念快照摘要 |
| `/decision/tree_status` | `sentry_decision_msgs/TreeStatus` | 行为树节点状态与 active path（transient local） |
| `/decision/intervention` | `sentry_decision_msgs/InterventionEvent` | 已应用的人工干预（可回放，见 §4.7） |

```bash
ros2 bag record /decision/state /decision/world_state /decision/tree_status /decision/intervention
```

`decision_node` 已接线以上发布；`decision_main` 无 ROS，不发布。

### 4.5 配置

配置采用「单一入口 + 按职责分文件」，详见 `docs/ARCHITECTURE.md` §15：

```text
config/
├── profiles.yaml           # 入口：map_profile / strategy_profile / pre_match
├── maps/<MAP>.yaml         # 命名点、frame
└── policies/<POLICY>.yaml  # 阈值、时间窗、兑换步长
```

XML 不写数值：坐标用命名点（`point="home"`），阈值用配置 key（`hp_key="nav.retreat_hp"`）。
启动时校验配置与行为树的引用一致性、数值完整解析、已知键的整数 / 非负约束；缺失或越界即启动报错并列出缺项，
生效配置打印为 `ACT` 日志。
`decision_node` 会把地图配置的 `frame_id` 注入 IO 节点的 `map_frame` 参数，保证 Nav2 目标坐标系一致。

### 4.6 裁判仿真与场景脚本

`referee_sim_node` 在本机模拟裁判系统与执行端，无需 MCU 与 navigation 仓库即可驱动 `decision_node` 走真实 IO 路径：

```bash
ros2 run sentry_decision_bringup decision_node     # 终端 1
ros2 run sentry_decision_sim referee_sim_node      # 终端 2：持续发布世界状态
```

它发布 `/sentry/*` 五条上行与 odom，提供 `NavigateToPose` action server，并把 `DecisionCommand` 按延迟回成 `DecisionAck`。

带场景脚本时按时间轴改世界，并在指定时刻断言 `/decision/state`，结束以退出码表示成败：

```bash
ros2 run sentry_decision_sim referee_sim_node --scenario \
  "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/full_match.yaml"
```

网页面板演示用一份稳定的非零世界（无断言、不退出）：

```bash
ros2 run sentry_decision_sim referee_sim_node --scenario \
  "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml" --hold
```

> 不带 `--scenario` 时裁判仿真发布的是全零世界（`referee_valid=true` 但所有数值为 0），
> 面板会显示 0；这不是故障。`--hold` 让场景时间轴跑完后继续发布最后一个世界状态。
> 演示世界从「未开始」起，比赛阶段由网页面板按钮或 `/sentry_sim/set_game_stage` 服务推进。

场景 YAML 结构（`full_match.yaml` 跑通巡逻→进攻→撤退→复活）：

```yaml
name: full_match
start_pose: [-11.47, -4.40, 0.0]  # 机器人初始位姿（可选，重置时也会恢复）
world:                          # 初始世界（时间轴之前应用）
  self_hp: 400
  self_ammo: 100
  enemy_outpost_hp: 1500
timeline:
  - at: 2.0
    expect:    { tactical_mode: patrol, has_nav_goal: true }
  - at: 3.0
    set_world: { game_time_remaining: 300 }
  - at: 6.0
    set_world: { self_hp: 40 }
  - at: 7.5
    expect:    { tactical_mode: retreat, nav_goal_x: -5.0, nav_goal_y: 3.0 }
```

`set_world` 字段见 `sentry_decision_sim/sim_world.hpp`；`expect` 支持 `tactical_mode`（名称或数字）、`stance`（名称或数字）、`has_nav_goal`、`nav_goal_x` / `nav_goal_y`、`has_cmd_vel`、`resource_ammo` / `resource_hp` / `resource_revive`。
`start_pose` 为可选的机器人初始位姿：进程启动与「重置」都会应用，避免默认停在场地中央。

`add_intent` 与 `disable` 经 `/decision/debug` 注入干预，把「人工干预」写成可提交的测试用例：

```yaml
  - at: 3.0
    add_intent: { field: nav_goal, value: "[7.0, 1.0]", lease_sec: 0 }
  - at: 5.0
    add_intent: { field: stance, value: "defense", lease_sec: 0 }
  - at: 7.0
    disable: { modules: "nav" }
```

端到端回归由 `tools/scenario_smoke_test.sh` 驱动（注册为 `scenario_full_match` 与 `scenario_intervention`）。

### 4.7 人工干预

`decision_node` 提供结构化干预入口（走同一仲裁，安全仍最高）：

```bash
# 通用调试 / 状态查询（service）
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_world', args: '{field: self_hp, value: 20}'}"
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'list_state', args: ''}"
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_module', args: '{module: nav, enabled: false}'}"

# 带 lease 的意图接管（action），goal 保持执行态直到过期或被取消
ros2 action send_goal /decision/manual_override \
  sentry_decision_msgs/action/ManualOverride \
  "{field: 0, value: '[1.0, 2.0]', lease_sec: 3.0, reason: 'manual'}"
```

`field` 与 `value` 语法、action / service 语义见 `docs/ARCHITECTURE.md` §14.3。
所有已应用的干预发布到 `/decision/intervention`，可随 rosbag 录制并在回放中复现（P3.3）。
冒烟测试：`tools/intervention_smoke_test.sh`（也注册为 `intervention_smoke`）。

### 4.8 网页面板（rosbridge）

`decision_node`（配合 `referee_sim_node` 或真实 IO）运行后，用 `viz.launch.py` 启动 rosbridge 与静态页：

```bash
ros2 launch sentry_decision_viz viz.launch.py   # rosbridge :9090 + 静态页 :8080
```

> 容器运行时需把端口映射到宿主：`docker run -p 8080:8080 -p 9090:9090 ...`；
> `docker/compose.yaml` 已配置这两个端口。否则宿主浏览器访问不到容器内的服务。

浏览器打开 `http://<宿主>:8080/`，点「连接」连到 `ws://<宿主>:9090`。页面显示：

- 左侧行为树（`/decision/tree_status`，RUNNING 高亮；已完成节点保留 SUCCESS / FAILURE，
  不会被 BT.CPP 的 tick 末重置刷成 IDLE）；
- 中间战场俯视图（RMUC2026 场地底图，来源导航仓库 `RMUC2026.pgm`，0.05 m/px、origin `[-14.6, -5.86]`；
  叠加己方位姿、导航目标、敌方位置），上方状态栏显示当前比赛阶段与 `MM:SS` 倒计时；
- 右侧 `WorldState` 与模块 / 活跃 Intent / 逐字段胜者 / 资源请求 / 最近动作 / 动作回执 /
  原始世界（每秒轮询 `list_state`）；
- 比赛阶段按钮：准备（3 分钟）/ 15s自检 / 5s倒计时 / 开始比赛（只可前进，当前及更早阶段自动禁用）；
  「重置」回未开始，并清空决策节点的干预 / 世界覆盖 / 模块开关；
- 人工干预按钮：强制撤退、切换模式、前往点位、兑换发弹 / 血量、模块启用 / 禁用、清空干预；
- 世界数值分两处：「仿真世界」直接改裁判仿真的真实世界（兑换 / 回血据此演变），
  「决策覆盖」只钉住决策视图（不改仿真世界，供离线 / 回放测试）。

> 决策只有收到 `game_status=4`（比赛中）才执行任务；未开始 / 准备 / 自检 / 倒计时 / 结算阶段输出
> `idle`，不下发任务导航目标，因此「准备阶段不动、开始比赛后才进攻/巡逻」。
> 比赛 420s 耗尽后阶段自动进入「比赛结算」（`game_status=5`），结算展示 10s 后停表。

比赛阶段服务由裁判仿真提供（真实比赛由裁判系统决定，故仅仿真用）：

```bash
ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
  "{stage: 4}"   # 0 重置 / 1 准备 / 2 15s自检 / 3 5s倒计时 / 4 比赛中
```

`stage` 除 0（重置）外必须大于当前阶段，否则返回 `success=false`；准备 / 自检 / 倒计时归零后
自动进入下一阶段。冒烟测试：`tools/match_smoke_test.sh`（也注册为 `match_smoke`）。

**右键「人工干预」区控件说明**：

| 控件 | 作用 | 底层调用 |
| --- | --- | --- |
| 比赛阶段 / 重置 | 推进阶段；重置回未开始并恢复机器人初始位姿 | `/sentry_sim/set_game_stage` |
| 暂停 / 继续 | 冻结比赛计时与机器人运动（odom 保持最后位置） | `/sentry_sim/set_game_pause` |
| 仿真世界 设置 | 直接改裁判仿真的真实世界：自身 / 基地 / 前哨血量、金币、发弹量、剩余时间 | `/sentry_sim/set_world` |
| 决策覆盖 设置 / 清除 | 只覆盖决策视图（不改仿真世界），用于离线 / 回放测试 | `/decision/debug set_world` / `clear_world` |
| 强制撤退 | 人工接管战术模式为 `retreat`，任务树当拍切到撤退 | `set_intent {field: tactical_mode, value: retreat}` |
| 模式 + 切换 | 把战术模式改成所选值（patrol/attack/defend/retreat/heal/respawn） | `set_intent {field: tactical_mode}` |
| 点位 + 前往 | 人工接管导航目标 `[x, y]`，优先于任务树 | `set_intent {field: nav_goal}` |
| 模块 启用 / 禁用 | 运行期关闭某模块后，该字段的意图（含人工干预）被丢弃 | `/decision/debug set_module` |
| 兑换发弹 / 血量 | 注入资源请求（ammo / hp） | `set_intent {field: resource_request}` |
| 清空干预 | 清空全部人工干预 / 世界覆盖 / 模块开关 | `/decision/debug clear_all` |

> 面板人工干预统一走 `/decision/debug` 的 `set_intent` service，不使用 `ManualOverride` action：
> vendored roslib 1.4.1 的 `ActionClient` 是 ROS 1 actionlib 命名（`/<action>/goal` 等），
> 无法对接 ROS 2 action 的 `/_action/*` 服务。action 接口本身仍可用
> `ros2 action send_goal /decision/manual_override ...` 直接调用。

> 「持续」一行的 `lease` 默认 **0**：人工接管一直生效，直到点「清空干预」或「重置」。
> 填正数则按秒到期后自动交还任务树（注意：lease 用系统时间计算，暂停期间仍会到期；
> 想让它跨暂停持续，请保持 0）。

> 「兑换发弹 / 血量」注入的是 `resource_request` 意图，由决策层转成一次性 `DecisionCommand`
> 动作下发给执行端，**不经过任务树**，所以行为树不会有分支变化；是否尝试 / 成功看右侧
> 「资源请求 / 最近动作 / 动作回执」，或决策节点日志的 `[action] 下发 ...` 与
> `[action] 动作 N 已确认`。

未进入「比赛中」（`game_status != 4`）时，人工意图按钮自动禁用，避免准备阶段误发干预；
仿真世界 / 决策覆盖与「清空干预」仍可用。决策覆盖只改数值、不提升 `referee.valid`，
不会绕过安全急停。**注意到「决策覆盖」会钉住该字段的决策视图**：即使仿真世界在补血 / 兑换，
面板血量也不会变；右侧「原始 血/弹/金」一行显示的才是覆盖前的真实世界。

> 兑换会真正结算进仿真世界，且**裁判侧会校验前置条件**：非法动作不改变世界、回执 `rejected`，
> 并在仿真日志打印「裁判拒绝动作 ...」。规则依据：
> - **兑换发弹量**（本地兑换，10 金币/10 发）要求**占领补给区 / 基地增益点 / 前哨站增益点**
>   （由 `event_code` 位段表示，仿真按机器人位置生成）；
> - **兑换血量**按规则只允许**脱战**远程兑换（`sentry_info_2.disengaged`）。
>
> 资源树按「立即复活 > 免费复活 > 远程兑换血量 > 本地兑换发弹量 > 远程兑换发弹量」求值
> （`tree/resource/root.xml`）：
> - **血量兑换按规则只走远程**（`IfDisengaged` + 金币 + `RequestRemoteHpExchange`），
>   本地血量兑换只保留给面板人工注入 / 调试；
> - **本地兑换发弹量**需占领增益点（`IfOccupyingGainPoint`）；不在增益点且脱战时改走
>   **远程兑换发弹量**（`IfNotOccupyingGainPoint`）；
> - **立即复活**需 `can_instant_resurrect` 且金币 ≥ 裁判给出的成本；否则确认免费复活。
>
> 金币不足 / 未脱战 / 不在增益点都会被裁判拒绝（`动作回执` 行显示 `rejected` 与原因）。
> 补给区自动回血：机器人处于补给区（默认 `healing` 点附近）且比赛中时，按上限血量的
> 10% / 秒回血，比赛 4 分钟后为 25%（仿真近似规则 5.2.1）。
> 补给区还会按规则 5.3.2 发放免费发弹量：**比赛每满 1 分钟累积 100 发，进入补给区时一次性
> 领取全部累积值**（与金币无关，金币不足时也能补弹）。
> 远程兑换按规则延迟生效：确认时扣金币，**6 秒后**才加发弹量 / 血量；远程兑换血量
> 在 6 秒内战亡则作废且金币不返还。相关次数 / 金币阈值见 `resource.remote_*_times` /
> `resource.remote_ammo_min_coins`。

人工意图与任务树走同一仲裁，安全层始终最高；它们只影响本 tick 起的输出，不会写回裁判数据。
「模式」接管只改变任务选择，不会伪造裁判的 `game_status`。

需要镜像包含 `ros-jazzy-rosbridge-suite`（见 `docker/Dockerfile`）。纯逻辑单测：
```bash
node src/sentry_decision_viz/web/test/format.test.mjs
```
端到端冒烟：`tools/viz_smoke_test.sh`（也注册为 `viz_smoke`；未装 rosbridge 时跳过）。

#### 4.8.1 场地标定工具（/calibrate.html）

用于在地图上标定 `config/maps/*.yaml` 的命名点坐标：左侧 RMUC 底图，右侧点位列表。

- 打开 `http://<宿主>:8080/calibrate.html`（与面板同一个静态服务，**不需要 rosbridge / 决策节点**）。
- 底图来自导航仓库 `RMUC2026.pgm`（583×300，0.05 m/px，origin `[-14.6, -5.86]`），
  世界坐标->像素映射集中在 `web/src/field.js`。
- 交互：点地图移动当前选中点 / 拖动标记 / 右侧改数值 / 添加或删除点；鼠标位置实时显示世界坐标。
  点位本地存 `localStorage`（键 `sentry.fieldPoints.v1`）。
- 「导出 YAML」生成可直接覆盖 `config/maps/RMUC26.yaml` 的 `points:` 片段；
  「从文本导入」可粘贴现有 yaml 片段继续编辑。
- 纯逻辑（`fieldExtent` / `worldToImage` / `imageToWorld`）由 `web/test/format.test.mjs` 覆盖。


### 4.9 离线回放（replay_main）

`decision_node` 与 `referee_sim_node` 运行时用 rosbag 录下上行与干预，赛后用 `replay_main` 离线重放：

```bash
ros2 bag record -o match_bag \
  /sentry/game_info /sentry/online_info /sentry/offline_info /sentry/team_info /sentry/radar_info \
  /aft_mapped_to_init /decision/intervention

ros2 run sentry_decision_bringup replay_main --bag match_bag
```

`replay_main` 读 `/sentry/*`（新格式）或旧标量话题，逐 tick 重跑行为树 / 仲裁 / 安全，并在原时刻注入录到的 `/decision/intervention`；以 `ACT` 日志输出现模式变化与最终统计。含干预的同一份 bag 两次回放逐 tick 一致（`replay_determinism`）。端到端冒烟：`tools/replay_smoke_test.sh`（也注册为 `replay_smoke`）。

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
ros2 run sentry_decision_bringup decision_main --plugin /path/to/libsentry_decision_nodes.so
```

### 5.2 decision_node 参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--rate` | `20.0` | tick 频率（Hz），取值 `(0, 1000]` |
| `--ticks` | `0` | 运行 tick 数，`0` 表示一直运行 |
| `--tree` | 安装后的 `tree/root.xml` | 行为树入口 XML 路径 |
| `--config` | 安装后的 `config/profiles.yaml` | 配置入口 YAML 路径 |
| `--plugin` | 空 | 节点插件 `.so` 路径 |
| `--groot2-port` | `0` | Groot2 监听端口，`0` 表示关闭（范围 `[0, 65535]`） |

### 5.3 io_node 参数

均为 ROS 参数，可用 `--ros-args -p name:=value` 覆盖。

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `map_frame` | `map` | 导航目标坐标系 |
| `game_info_topic` | `/sentry/game_info` | 比赛宏观信息（GameInfo） |
| `online_info_topic` | `/sentry/online_info` | 自身在线状态（SentryInfoOnline） |
| `offline_info_topic` | `/sentry/offline_info` | 自身视觉 / 形态（SentryInfoOffline） |
| `team_info_topic` | `/sentry/team_info` | 队伍信息（TeamInfo） |
| `radar_info_topic` | `/sentry/radar_info` | 雷达 / 敌方信息（RadarInfo） |
| `decision_ack_topic` | `/sentry/decision_ack` | 动作回执（DecisionAck） |
| `decision_command_topic` | `/sentry/decision_command` | 决策下行（DecisionCommand） |
| `odom_topic` | `/aft_mapped_to_init` | 里程计（Odometry） |
| `cmd_vel_topic` | `cmd_vel` | 速度指令（Twist） |
| `navigate_action` | `navigate_to_pose` | 导航 action（NavigateToPose） |

```bash
ros2 run sentry_decision_io io_node --ros-args \
  -p odom_topic:=/sentry/odom \
  -p navigate_action:=/navigate_to_pose
```

### 5.4 referee_sim_node 参数

上行 / 下行话题与 `io_node` 同名同默认，另加：

| 参数 / 命令行 | 默认 | 说明 |
| --- | --- | --- |
| `--rate` | `20.0` | 发布与 tick 频率（Hz），取值 `(0, 1000]` |
| `--scenario` | 空 | 场景 YAML 路径；为空则持续发布全零世界 |
| `--hold` | 关 | 场景时间轴跑完后不退出，保持最后一个世界状态（面板演示） |
| `--ros-args -p set_game_stage_service` | `/sentry_sim/set_game_stage` | 比赛阶段设置服务名 |
| `--ros-args -p set_game_pause_service` | `/sentry_sim/set_game_pause` | 暂停 / 恢复服务名 |
| `--ros-args -p set_world_service` | `/sentry_sim/set_world` | 直接修改仿真世界的服务名 |
| `--ros-args -p max_hp` | `400` | 哨兵上限血量（用于兑换 / 回血上限） |
| `--ros-args -p supply_center_x` / `_y` | `-12.36` / `-3.47` | 补给区圆心（默认取地图 `healing` 点） |
| `--ros-args -p supply_radius` | `1.5` | 补给区半径（米） |
| `--ros-args -p supply_heal_ratio` | `0.10` | 补给区回血比例（上限血量 / 秒） |
| `--ros-args -p supply_heal_ratio_late` | `0.25` | 比赛 4 分钟后的回血比例 |
| `--ros-args -p supply_heal_late_after_s` | `240` | 提高回血比例的已进行秒数 |
| `--ros-args -p base_buff_center_x` / `_y` / `_radius` | `-11.47` / `-4.40` / `1.0` | 己方基地增益点区域（默认 `home` 点；半径 0 = 不启用） |
| `--ros-args -p our_outpost_center_x` / `_y` / `_radius` | `-2.55` / `-2.21` / `1.0` | 己方前哨站增益点区域（默认取地图 `our_outpost` 点） |
| `--ros-args -p fort_buff_center_x` / `_y` / `_radius` | `-7.37` / `1.60` / `1.0` | 己方堡垒增益点区域（默认 `fort` 点） |
| `--ros-args -p decision_state_topic` | `/decision/state` | 场景断言订阅的决策状态话题 |
| `--ros-args -p odom_topic` | `/aft_mapped_to_init` | 里程计发布话题 |
| `--ros-args -p navigate_action` | `navigate_to_pose` | 提供的导航 action 名 |

### 5.5 replay_main 参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `--bag` | 必填 | rosbag2 路径（含 `/sentry/*` 上行与 `/decision/intervention`） |
| `--ticks` | `0` | 最多回放 tick 数，`0` 表示回放到数据结束 |
| `--rate` | `20.0` | 回放步长频率（Hz），取值 `(0, 1000]` |
| `--odom-topic` | `/aft_mapped_to_init` | 里程计话题 |
| `--tree` / `--config` | 安装后的路径 | 行为树 / 配置入口 |

## 6. 测试

| 范围 | 命令 |
| --- | --- |
| 宿主 core 单测（无需 ROS） | `tools/host_core_test.sh` |
| 容器全量 | `docker/entrypoint.sh test` |
| io 冒烟 | 容器内 `tools/io_smoke_test.sh` |
| 场景 / 干预 / 回放 / 面板 / 比赛阶段 / 世界结算冒烟 | `tools/scenario_smoke_test.sh`、`tools/intervention_smoke_test.sh`、`tools/replay_smoke_test.sh`、`tools/viz_smoke_test.sh`、`tools/match_smoke_test.sh`、`tools/sim_effects_smoke_test.sh` |
| 网页面板纯逻辑单测 | `node src/sentry_decision_viz/web/test/format.test.mjs` |
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
