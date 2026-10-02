# 单个行为树节点的状态快照，由 TreeStatePublisher 采集，用于可视化。

# 节点在树中的完整路径，如 /MissionRoot/Attack/GotoNamedPoint
string full_path
# 注册名（节点类型），如 GotoNamedPoint
string registration_name
# 实例名（XML name 属性），可为空
string instance_name
# 与 BT::NodeStatus 1:1：0 IDLE / 1 RUNNING / 2 SUCCESS / 3 FAILURE / 4 SKIPPED
uint8 status
