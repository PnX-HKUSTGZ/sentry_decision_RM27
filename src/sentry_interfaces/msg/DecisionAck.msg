# 下位机对决策命令的执行回执（上行）。
std_msgs/Header header

uint32 request_id  # 对应 DecisionCommand.request_id
bool accepted      # 是否接受 / 执行成功
uint8 code         # 结果码，0 表示成功，其余含义待定
string detail      # 可读失败原因
