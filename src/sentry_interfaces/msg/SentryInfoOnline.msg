# 哨兵自身在线状态（上行）。
std_msgs/Header header

uint16 self_health
uint16 bullets_remaining
uint16 cooling_value
uint16 heat_limit
uint16 current_heat
geometry_msgs/Point sentry_pos
float32 speed_monitor_angle  # 测速模块朝向，单位 deg，正北为 0
uint32 sentry_info_1         # 原始位段（兑换 / 复活信息）
uint16 sentry_info_2         # 原始位段（脱战 / 可兑换弹量 / 姿态）
uint64 sentry_info_3         # 原始位段（姿态强化时间，暂不解码）
uint8 energy_ratio           # 底盘能量比例
