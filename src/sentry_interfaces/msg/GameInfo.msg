# 比赛宏观信息（上行）。Phase 1 只填部分字段，其余留空待 MCU 扩展。
std_msgs/Header header

uint8 game_status          # 0 未开始 / 1 准备 / 2 自检 / 3 倒计时 / 4 比赛中 / 5 结算
uint16 game_time_remaining  # 剩余时间，单位 s
uint16 coin_remaining       # 己方剩余金币
uint32 event_code           # 场地事件代码（原始，未解码）
uint8 detect_color          # 红蓝方，编码待确认，当前留空
bool can_rebuild_outpost    # 己方是否可重建前哨站

float32 manual_point_x      # 手动目标点 x，单位 m，minimap 系
float32 manual_point_y      # 手动目标点 y
uint8 manual_key            # 手动按键

uint16 enemy_outpost_hp     # 敌方前哨站血量
uint16 enemy_base_hp        # 敌方基地血量
