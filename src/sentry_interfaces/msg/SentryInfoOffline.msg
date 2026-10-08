# 哨兵自身离线 / 视觉与形态状态（上行）
# 由本机传感器获得，不依赖裁判系统
std_msgs/Header header

# 自瞄相关
bool is_get                             # 视觉是否锁定敌人
geometry_msgs/Point armor_pos           # 瞄准装甲板位置
uint8 armor_num                         # 装甲板编号

# 过洞/变形相关 - 暂未启用，仅供参考
# uint8 lifter_current_pos                # 0 上 / 1 下 / 2 中
# bool is_transformable
# float32 transform_state                 # 0~1
# bool tunnel_yaw_aligned

# 超级电容
uint8 capacitor_capacity                # 电容容量百分比
