# 一次已应用的干预，发布到 /decision/intervention，供记录与回放。
# 与 core::InterventionController 的动作一一对应。

std_msgs/Header header

uint8 KIND_INTENT = 0
uint8 KIND_CLEAR_INTENT = 1
uint8 KIND_WORLD_OVERRIDE = 2
uint8 KIND_CLEAR_WORLD = 3
uint8 KIND_MODULE_SWITCH = 4
uint8 KIND_CLEAR_ALL = 5

uint8 kind
# IntentField，kind 为 intent / clear_intent 时有效
uint8 field
# 类型化取值（YAML/JSON 文本），原样透传供回放
string value
float64 lease_sec
string module
bool enabled
string reason
