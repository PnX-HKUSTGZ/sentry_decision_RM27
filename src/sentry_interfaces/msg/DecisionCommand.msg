# 决策下发给下位机的统一命令（下行）。每条命令只携带一个动作。
# request_id 用于日志关联；同一目标的重试复用同一个 id（下发值是幂等的累计目标值）。
std_msgs/Header header

uint32 request_id
uint8 task_state            # 0 idle / 1 navigate / 2 scan / 3 outpost / 4 resource / 5 heal / 6 respawn
DecisionAction action
