# 仲裁后的唯一输出，是 core::DecisionOutput 的 ROS 投影。

bool has_nav_goal
geometry_msgs/Point nav_goal
float64 nav_goal_yaw

bool has_cmd_vel
geometry_msgs/Twist cmd_vel

# TacticalMode: 0 unknown / 1 patrol / 2 attack / 3 defend / 4 retreat / 5 heal / 6 respawn
uint8 tactical_mode

# ResourceRequest
uint16 resource_ammo
uint16 resource_hp
bool resource_revive
