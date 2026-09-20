# 哨兵自身离线 / 视觉与形态状态（上行）。
std_msgs/Header header

bool is_get                             # 视觉是否锁定敌人
geometry_msgs/Point armor_pos           # 瞄准装甲板位置
uint8 armor_num                         # 装甲板编号
float32 yaw_camerainit_to_gimbal
uint8 lifter_current_pos                # 0 上 / 1 下 / 2 中
bool is_transformable
float32 transform_state                 # 0~1
uint8 capacitor_capacity                # 电容容量百分比
bool tunnel_yaw_aligned
