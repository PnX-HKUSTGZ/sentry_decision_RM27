# 行为树一次 tick 后的状态快照，发布到 /decision/tree_status，供网页面板与记录使用。

std_msgs/Header header

# 决策 tick 序号
uint32 tick
# 本拍行为树 tick 的耗时（毫秒）
float64 tick_ms
# 全部节点的状态
TreeNodeStatus[] nodes
# RUNNING 节点的 full_path（运行中节点的祖先必然也在运行，故等价于 active path），用于高亮当前执行分支
string[] active_path
