# 仲裁后的唯一输出，是 core::DecisionOutput 的 ROS 投影。

bool has_nav_goal
geometry_msgs/Point nav_goal
float64 nav_goal_yaw

bool has_cmd_vel
geometry_msgs/Twist cmd_vel

# TaskState: 0 idle / 1 navigate / 2 scan / 3 outpost / 4 resource / 5 heal / 6 respawn
uint8 task_state

# SentryStance: 0 unknown / 1 attack / 2 defense / 3 move
uint8 stance
