# 一次 tick 的决策快照，发布到 /decision/state。

std_msgs/Header header

uint32 tick
DecisionOutput output
uint32 conflict_count
string[] warnings

bool referee_valid
bool self_valid
bool nav_valid
