# 仿真与可视化

> 面向使用者：本地仿真、场景脚本、裁判仿真世界语义、网页面板与场地标定工具的操作指南。
> 设计动机与内部实现见 [ARCHITECTURE.md](ARCHITECTURE.md) §14；命令参数速查见 [USAGE.md](USAGE.md)。

## 1. 一键演示 tools/demo.sh

一条命令拉起「网页面板 + 裁判仿真 + 决策节点」，不想每次开三个终端时使用：

```bash
tools/demo.sh            # 宿主直接运行；自动起容器并映射 8080/9090
```

- 三个进程的 stdout 汇到当前终端，分别加 `[viz]` / `[sim]` / `[decision]` 前缀；
  同时各写一份到 `.docker-build/demo_logs/<name>.log`，可另开终端 `tail -f`。
- `Ctrl+C` 停止全部（脚本会终止整组进程）。
- 面板地址 `http://localhost:8080`。

| 命令 | 作用 |
| --- | --- |
| `tools/demo.sh --no-viz` | 只起裁判仿真 + 决策（不起 rosbridge / 静态页） |
| `tools/demo.sh --build` | 先 `colcon build` 再启动 |
| `tools/demo.sh --scenario <容器内路径>` | 指定场景（默认 `demo.yaml`） |
| `tools/demo.sh --rate 20` | 仿真频率 |
| `tools/demo.sh --with-nav` | 双仓库联调：额外起真实导航容器（见 §8） |
| `tools/demo.sh --with-nav --nav-silent` | 同上，导航不开 RViz |

已经在容器里时同样可用（脚本会自动识别容器环境），也可用 `docker/entrypoint.sh demo`。

## 2. 本地最小闭环

```bash
ros2 run sentry_decision_bringup decision_main
```

dummy 裁判系统 + 伪导航驱动信念层，无需 MCU 与导航仓库。参数见 [USAGE.md](USAGE.md#51-decision_main)。

## 3. 裁判仿真 referee_sim_node

`referee_sim_node` 在本机模拟裁判系统与执行端，无需 MCU 与导航仓库即可驱动 `decision_node` 走真实 IO 路径。
它发布 `/sentry/*` 四条上行与 odom，提供 `NavigateToPose` action server，并把 `DecisionCommand`
按延迟结算进仿真世界；决策侧再通过上行数据观察到效果，与实车的「下发 → 观测 → 校验」一致。

```bash
ros2 run sentry_decision_bringup decision_node     # 终端 1
ros2 run sentry_decision_sim referee_sim_node      # 终端 2：持续发布世界状态
```

> 不接 MCU、只测决策 ↔ auto-aim 的 ROS 通信时，用本节点扮演上游即可：
> 它发的是同一批 `sentry_interfaces` 消息，决策侧无需改动。

### 3.1 命令行参数

上行 / 下行话题与 `io_node` 同名同默认。数值参数（`max_hp` / 回血比例 / 区域引用 / 导航速度）来自
`config/sim.yaml`；区域（补给区 / 增益点）的**圆心与半径**取自其 `map:` 引用的命名点
（半径 = 点第 4 位），`-p` 可逐项覆盖。

| 参数 / 命令行 | 默认 | 说明 |
| --- | --- | --- |
| `--rate` | `20.0` | 发布与 tick 频率（Hz），取值 `(0, 1000]` |
| `--scenario` | 空 | 场景 YAML 路径；为空则持续发布全零世界 |
| `--hold` | 关 | 场景时间轴跑完后不退出，保持最后一个世界状态（面板演示） |
| `--sim-config` | 编译期绝对路径 | 仿真参数 YAML；该路径不存在时回退到 `config/sim.yaml` |
| `-p set_game_stage_service` | `/sentry_sim/set_game_stage` | 比赛阶段设置服务名 |
| `-p set_world_service` | `/sentry_sim/set_world` | 直接修改仿真世界的服务名 |
| `-p apply_effect_service` | `/sentry_sim/apply_effect` | 施加具名仿真效果的服务名 |
| `-p max_hp` | `400` | 哨兵上限血量（用于兑换 / 回血上限） |
| `-p supply_center_x` / `_y` | 地图 `sim_home` 点 | 补给区圆心 |
| `-p supply_radius` | 地图 `sim_home` 第 4 位（`1.5`） | 补给区半径（米） |
| `-p supply_enter_delay_s` | `1.0` | 进入补给区后延迟多少秒才开始回血 |
| `-p supply_heal_ratio` | `0.10` | 补给区回血比例（上限血量 / 秒） |
| `-p supply_heal_ratio_late` | `0.25` | 比赛 4 分钟后的回血比例 |
| `-p supply_heal_late_after_s` | `240` | 提高回血比例的已进行秒数 |
| `-p base_buff_center_x` / `_y` / `_radius` | 地图 `sim_base` 点及其第 4 位 | 己方基地增益点（仅兑换；半径 0 = 不启用） |
| `-p our_outpost_center_x` / `_y` / `_radius` | 地图 `sim_our_outpost` 点及其第 4 位 | 己方前哨站增益点（仅兑换） |
| `-p fort_buff_center_x` / `_y` / `_radius` | 地图 `sim_fort` 点及其第 4 位 | 己方堡垒增益点 |
| `-p decision_state_topic` | `/decision/state` | 场景断言订阅的决策状态话题 |
| `-p odom_topic` | `/aft_mapped_to_init` | 里程计发布话题 |
| `-p navigate_action` | `navigate_to_pose` | 提供的导航 action 名 |
| `-p provide_nav` | `true` | 是否提供伪导航（action server + odom）；双仓库设 `false` |
| `-p decision_world_state_topic` | `/decision/world_state` | `provide_nav=false` 时位姿来源 |

### 3.2 场景脚本

带场景脚本时按时间轴改世界，并在指定时刻断言 `/decision/state`，结束以退出码表示成败：

```bash
# 完整对局：进攻 → 高地 → 后方巡逻 → 守堡垒 → 撤退 → 复活
ros2 run sentry_decision_sim referee_sim_node --scenario \
  "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/full_match.yaml"

# 面板演示：稳定非零世界（无断言、不退出）
ros2 run sentry_decision_sim referee_sim_node --scenario \
  "$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml" --hold
```

场景 YAML 结构（`full_match.yaml` 片段）：

```yaml
name: full_match
start_pose: [-11.47, -4.40, 0.0]  # 机器人初始位姿（可选，重置时也会恢复）
world:                          # 初始世界（时间轴之前应用）
  self_hp: 400
  self_ammo: 100
  enemy_outpost_hp: 1500
timeline:
  - at: 2.0
    expect:    { tactical_mode: attack, has_nav_goal: true }   # 敌前哨存活 -> 进攻
  - at: 3.0
    set_world: { enemy_outpost_hp: 0 }
  - at: 4.5
    expect:    { tactical_mode: patrol, nav_goal_x: 2.49, nav_goal_y: 4.05 }  # -> 高地巡逻
  - at: 9.0
    set_world: { game_time_remaining: 100 }        # 双方前哨皆毁且剩余 <= 180s
  - at: 10.5
    expect:    { tactical_mode: defend, nav_goal_x: -7.37, nav_goal_y: 1.60 }  # -> 守堡垒
```

- `set_world` 字段见 `sentry_decision_sim/sim_world.hpp`；
- `expect` 支持 `tactical_mode`（名称或数字）、`stance`（名称或数字）、`has_nav_goal`、
  `nav_goal_x` / `nav_goal_y`、`has_cmd_vel`、`resource_ammo` / `resource_revive` / `resource_instant_revive`；
- `start_pose` 为可选的机器人初始位姿：进程启动与「重置」都会应用，避免默认停在场地中央。

端到端回归由 `tools/scenario_smoke_test.sh` 驱动（注册为 `scenario_full_match`）。

> 不带 `--scenario` 时裁判仿真发布的是全零世界（`upstream_valid=true` 但所有数值为 0），面板会显示 0，
> 这不是故障。`--hold` 让场景时间轴跑完后继续发布最后一个世界状态。

### 3.3 服务

| 服务 | 作用 |
| --- | --- |
| `/sentry_sim/set_game_stage` | 推进比赛阶段（0 重置 / 1 准备 / 2 15s自检 / 3 5s倒计时 / 4 比赛中） |
| `/sentry_sim/set_world` | 直接改仿真世界真实值（血量 / 金币 / 发弹量 / 剩余时间等） |
| `/sentry_sim/apply_effect` | 施加具名效果，步长来自 `config/sim.yaml` 的 `effects` |

```bash
ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage "{stage: 4}"
```

`stage` 除 0（重置）外必须大于当前阶段，否则返回 `success=false`；准备 / 自检 / 倒计时归零后
自动进入下一阶段。冒烟测试：`tools/match_smoke_test.sh`。

## 4. 仿真世界语义

以下为裁判仿真对 2027 规则（V1.0.0）的近似实现，用于本地闭环；真实比赛由裁判系统决定。

### 4.1 主策略与比赛阶段

主策略 `RuleBasedStrategicPolicy` 还原上一赛季打法：

1. **敌方前哨存活** → 进攻敌方前哨（`kAttack`）；
2. **敌方前哨被毁、我方前哨存活** → 中央高地 3 点循环巡逻（`kPatrol`）；
3. **双方前哨皆毁** → 剩余时间 > `strategic.fort_after_remaining_s`（默认 180s）后方巡逻，否则回堡垒防守（`kDefend`）。

低血撤退 / 低弹补给 / 阵亡复活的优先级高于主策略。巡逻对当前命名点持续发目标，到点后停留
`nav.patrol_dwell_s` 秒再去下一点。

决策只在 `game_status=4`（比赛中）执行任务；其余阶段输出 `idle`、不下发任务导航目标，
因此「准备阶段不动、开始比赛后才进攻 / 巡逻」。比赛 420s 耗尽后进入结算（`game_status=5`）。

### 4.2 撤退迟滞

撤退带**迟滞**：血量低于 `nav.retreat_hp` 进入撤退并在补给区回血，保持到恢复到 `nav.recovery_hp`
才离开（把 `nav.recovery_hp` 设为哨兵上限血量即「回满才走」），避免刚过进入阈值就切回主任务。

### 4.3 补给区回血与免费发弹量

- **回血**（规则 5.2.1）：机器人处于补给区且比赛中时，按上限血量的 10% / 秒回血，比赛 4 分钟后为 25%；
  进入补给区后先等 1s（`supply_enter_delay_s`）再开始。
- **免费发弹量**（规则 5.3.2）：比赛每满 1 分钟累积 100 发，进入补给区时一次性领取全部累积值
  （与金币无关，金币不足也能补弹）。
- 基地 / 前哨站增益点只用于兑换，不触发回血。

### 4.4 资源兑换与复活

兑换会真正结算进仿真世界，且**裁判侧会校验前置条件**：非法动作不改变世界，
并在日志打印「裁判拒绝动作 ...」。资源树按
「立即复活 > 免费复活 > 远程兑换血量 > 本地兑换发弹量 > 远程兑换发弹量」求值（`tree/resource/root.xml`）。

| 动作 | 前置条件 | 结算 |
| --- | --- | --- |
| 本地兑换发弹量 | 占领补给区 / 基地 / 前哨站增益点 | 10 金币 / 10 发 |
| 远程兑换发弹量 | 脱战；不在增益点时走此路 | 确认扣金币，**6 秒后**才加发弹量 |
| 远程兑换血量 | 脱战（规则只允许远程） | 确认扣金币，6 秒后才加血量；6 秒内战亡作废且金币不返还 |
| 确认免费复活 | 待复活且 `info1.can_free_resurrect`（bit 19） | 读条完成且已确认时以 **10% 上限血量**复活 |
| 立即复活 | 待复活、可立即复活且金币 ≥ 成本 | 回满血，本局累计立即复活次数 +1 |

- **复活读条**（规则 5.2.2）：战亡后裁判立即开始读条，总长
  `10 + round((420-战亡时剩余秒数)/10) + 20*本局累计立即复活次数`，战亡瞬间锁定；
  每秒 +1，在己方补给区增益点或己方基地血量 <4000 时每秒 +4。读条状态是仿真专属（`SimWorld::respawn`），不进入上行消息。
- 相关次数 / 金币阈值见 `resource.remote_*_times` / `resource.remote_ammo_min_coins`。
- **累计值与幂等**：下发的 `value` 是 0x0120 的累计目标值（发弹量 / 远程次数）或电平位（复活）。
  重复下发同一目标只按增量结算，不会重复扣金币；目标回退会被拒绝。
- 冒烟测试：`tools/sim_effects_smoke_test.sh`。

> 金币不足 / 未脱战 / 不在增益点都会被裁判拒绝，日志会打印原因；决策侧则通过上行数据判断是否生效并重试。

### 4.5 增益点占领

节点每拍按机器人位姿判定可配置的增益点区域（补给区 / 基地 / 前哨站 / 堡垒）是否被占领，
写回 `event_code` 位段（`supply_zone_occupied` 等），供决策端 `IfOccupyingGainPoint` 门控。
区域圆心与半径来自地图点（半径 = 点第 4 位），四个区域默认启用，可 `-p *_radius:=0` 关闭。
`sentry_info_2.disengaged` 默认脱战，可用 `set_world {disengaged: 0}` 覆盖。

## 5. 战术层覆盖

调试只保留「仿真世界」与「战术层」两类入口，决策内部不提供人工写入口。战术层覆盖通过 `/decision/debug`：

```bash
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_tactical_mode', args: '{mode: retreat, lease_sec: 0}'}"
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'clear_tactical_mode', args: ''}"
ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'list_state', args: ''}"
```

覆盖在战略层求值之后、任务树读取之前写入 `context.strategy.mode`，因此会改变任务树分支；
仅在比赛中（`game_status=4`）生效；安全层优先级不受影响。`lease_sec` 为 0 表示不过期，
填正数则按秒到期后自动交还战略层（用系统时间计算）。语义见 [ARCHITECTURE.md](ARCHITECTURE.md) §10 / §14.3。

## 6. 网页面板

`decision_node`（配合 `referee_sim_node` 或真实 IO）运行后，用 `viz.launch.py` 启动 rosbridge 与静态页：

```bash
ros2 launch sentry_decision_viz viz.launch.py   # rosbridge :9090 + 静态页 :8080
```

> 容器运行时需把端口映射到宿主：`docker run -p 8080:8080 -p 9090:9090 ...`。
> 浏览器打开 `http://<宿主>:8080/`，点「连接」连到 `ws://<宿主>:9090`。

页面显示：

- 左侧行为树（`/decision/tree_status`，RUNNING 高亮；已完成节点保留 SUCCESS / FAILURE）；
- 中间战场俯视图（RMUC2026 场地底图，0.05 m/px、origin `[-14.6, -5.86]`；
  叠加己方位姿、导航目标、敌方位置），上方状态栏显示比赛阶段与倒计时；
- 右侧 `WorldState` 与资源请求 / 最近动作 / 动作结果（每秒轮询 `list_state`）；
- 比赛阶段按钮、仿真世界直接设置、仿真效果三行按钮、战术层覆盖下拉。

**面板控件与底层调用**：

| 控件 | 作用 | 底层调用 |
| --- | --- | --- |
| 比赛阶段 / 重置 | 推进阶段；重置回未开始并恢复机器人初始位姿 | `/sentry_sim/set_game_stage` |
| 仿真世界 设置 | 直接改裁判仿真的真实世界：血量、金币、发弹量、剩余时间 | `/sentry_sim/set_world` |
| 仿真效果 三行按钮 | 自身扣血 / 扣弹 / 死亡；我方 / 敌方前哨扣血 / 摧毁、基地扣血 | `/sentry_sim/apply_effect` |
| 模式 + 设置 / 清除 | 设置或清除战术层覆盖（patrol/attack/defend/retreat/heal/respawn） | `/decision/debug set_tactical_mode` / `clear_tactical_mode` |

- 比赛阶段按钮：准备（3 分钟）/ 15s自检 / 5s倒计时 / 开始比赛（只可前进，当前及更早阶段自动禁用）；
  「重置」回未开始并清除战术层覆盖。双仓库（`--with-nav`）下重置会额外下发一个「回起点」导航目标，
  独立模式仍是直接瞬移。
- 未进入「比赛中」时战术层覆盖按钮自动禁用；仿真世界 / 仿真效果仍可用。

纯逻辑单测与冒烟：
```bash
node src/sentry_decision_viz/web/test/format.test.mjs
tools/viz_smoke_test.sh     # 未装 rosbridge 时跳过
```

## 7. 场地标定工具（/calibrate.html）

用于在地图上标定 `config/maps/*.yaml` 的命名点坐标：左侧 RMUC 底图，右侧点位列表。

- 打开 `http://<宿主>:8080/calibrate.html`（与面板同一个静态服务，**不需要 rosbridge / 决策节点**）。
- 初始为**空序列**（工具不读取真实地图文件），避免把示例点误当成实际配置；点「添加点」或
  「从文本导入」开始标定。点位本地存 `localStorage`（键 `sentry.fieldPoints.v1`），「重置」清空。
- 底图来自导航仓库 `RMUC2026.pgm`（583×300，0.05 m/px，origin `[-14.6, -5.86]`），
  世界坐标 → 像素映射集中在 `web/src/field.js`。
- 交互：点地图移动当前选中点 / 拖动标记 / 右侧改数值 / 添加或删除点；鼠标位置实时显示世界坐标。
- 列表可直接编辑 x/y/yaw 与 `r`（区域半径，米，0 = 普通点）；`r > 0` 的点按半径画出青色虚线环，
  导出时写出第 4 位。编辑即时重绘，也可点「刷新预览」强制重绘。
- 纯逻辑（`fieldExtent` / `worldToImage` / `imageToWorld`）由 `web/test/format.test.mjs` 覆盖。

## 8. 双仓库联调（真实导航）

决策本地仿真与真实导航栈（Gazebo + Point-LIO + Nav2）联调：

```bash
tools/demo.sh --with-nav --nav-silent
```

接口契约、关键决策与运行细节见 [sim_dual_repo.md](sim_dual_repo.md)。
