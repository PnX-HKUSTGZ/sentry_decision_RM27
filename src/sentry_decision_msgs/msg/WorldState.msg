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
bool can_rebuild_outpost
bool referee_valid

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

# 敌方（RadarInfo / SentryInfoOffline）
bool enemy_valid
bool enemy_target_valid
bool has_enemy_position
float64 enemy_x
float64 enemy_y
uint8 enemy_count

# 队友（TeamInformation）
uint8 ally_count
