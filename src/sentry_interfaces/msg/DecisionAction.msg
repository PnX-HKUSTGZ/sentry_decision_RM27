# 单个决策动作。mode 决定语义，配置动作时必须显式选择。
uint8 MODE_ONE_SHOT = 0  # 边沿触发，执行一次即完成
uint8 MODE_POLLED = 1    # 轮询，按 interval_ms 重发，以最新值为准

uint8 KIND_NONE = 0
uint8 KIND_AMMO_EXCHANGE = 1         # 本地兑换允许发弹量，value = 数量
uint8 KIND_HP_EXCHANGE = 2           # 本地兑换血量，value = 数量
uint8 KIND_FREE_RESURRECT = 3        # 确认免费复活
uint8 KIND_INSTANT_RESURRECT = 4     # 兑换立即复活
uint8 KIND_REMOTE_AMMO_EXCHANGE = 5  # 远程兑换发弹量，value = 次数
uint8 KIND_REMOTE_HP_EXCHANGE = 6    # 远程兑换血量，value = 次数

uint8 kind
uint8 mode
uint16 interval_ms  # 仅 MODE_POLLED 使用，0 表示由发送方默认节拍
int32 value         # 数量 / 次数等，含义随 kind
