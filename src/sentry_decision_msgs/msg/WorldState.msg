# 决策可见的世界状态摘要，发布到 /decision/world_state，供可视化与 rosbag 记录。

std_msgs/Header header

# 比赛宏观（GameInfo / TeamInformation）
int32 game_status
int32 game_time_remaining
int32 coins
int32 self_hp
int32 self_ammo
int32 base_hp
int32 our_outpost_hp
int32 enemy_outpost_hp
int32 enemy_base_hp
# 2027 前哨阵地控制状态：0 我方完全 / 1 我方临时 / 2 敌方控制（0x0101 不区分敌方完全/临时）
uint8 our_outpost_control
uint8 enemy_outpost_control
# 前哨站建筑归属：0 未知 / 1 我方 / 2 敌方（由决策层根据重建维护）
uint8 our_outpost_owner
uint8 enemy_outpost_owner
bool can_rebuild_outpost
bool upstream_valid

# 当前物理姿态 SentryStance: 0 unknown / 1 attack / 2 defense / 3 move（SentryInfoOnline.info2）
uint8 stance

# 自身 / 定位（SentryInfoOnline / odom）
float64 pos_x
float64 pos_y
float64 yaw
float64 vx
float64 vy
float64 wz
bool self_valid

# 导航
bool nav_valid
bool has_nav_goal
float64 nav_goal_x
float64 nav_goal_y
bool nav_reached
bool nav_failed

# 敌方（SentryInfoOffline）
bool enemy_valid
bool enemy_target_valid
bool has_enemy_position
float64 enemy_x
float64 enemy_y

# 队友（TeamInformation）
uint8 ally_count
