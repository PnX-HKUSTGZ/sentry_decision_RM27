# 决策下发给下位机的统一命令（下行）。Phase 1 每个命令携带一个动作。
std_msgs/Header header

uint32 request_id    # 唯一请求号，用于与 DecisionAck 对应
DecisionAction action
