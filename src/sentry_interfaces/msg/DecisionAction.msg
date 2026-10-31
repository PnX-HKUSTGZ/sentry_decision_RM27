# 单个决策动作。value 的含义随 kind：
#   - 兑换类（1/4/5）：协议 0x0120 要求的「累计目标值」（本地兑换 = 本场累计兑换发数，
#     远程兑换 = 累计请求次数），不是本次增量；
#   - 复活类（2/3）：0x0120 的电平位，1 = 置位（请求复活），0 = 清零。
uint8 KIND_NONE = 0
uint8 KIND_AMMO_EXCHANGE = 1         # 本地兑换允许发弹量，value = 本场累计兑换发数
uint8 KIND_FREE_RESURRECT = 2        # 确认免费复活，value = 1/0
uint8 KIND_INSTANT_RESURRECT = 3     # 兑换立即复活，value = 1/0
uint8 KIND_REMOTE_AMMO_EXCHANGE = 4  # 远程兑换发弹量，value = 本场累计请求次数
uint8 KIND_REMOTE_HP_EXCHANGE = 5    # 远程兑换血量，value = 本场累计请求次数

uint8 kind
int32 value
