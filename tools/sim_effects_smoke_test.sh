#!/usr/bin/env bash
# 容器内冒烟：裁判仿真的世界结算——兑换发弹真正加弹 / 扣金币，补给区自动回血。
set -euo pipefail

# 使用独立 ROS_DOMAIN_ID，避免与同批次其它集成测试的节点互相干扰。
export ROS_DOMAIN_ID="${SIM_EFFECTS_DOMAIN_ID:-46}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY="${SCENARIO_OVERLAY_DIR:-${ROOT}/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${OVERLAY}/setup.bash" ]]; then
  source "${OVERLAY}/setup.bash"
fi
set -u

DECISION_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/decision_node"
SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"
SCENARIO="$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml"

"${DECISION_BIN}" --ticks 0 >/tmp/effects_decision.log 2>&1 & DP=$!
# 把补给区圆心设到原点并放大半径，使 demo 的初始位姿必然处于补给区，便于验证回血。
"${SIM_BIN}" --scenario "${SCENARIO}" --hold \
  --ros-args -p supply_center_x:=0.0 -p supply_center_y:=0.0 -p supply_radius:=10.0 \
  -p max_hp:=400 -p supply_heal_late_after_s:=240 \
  >/tmp/effects_sim.log 2>&1 & SP=$!
trap 'kill "${DP}" "${SP}" 2>/dev/null || true' EXIT
sleep 3

# 仿真节点若因参数 / 依赖问题启动失败，尽早暴露，避免卡在 service 调用。
if ! kill -0 "${SP}" 2>/dev/null; then
  echo "FAIL: referee_sim_node 未启动" >&2
  cat /tmp/effects_sim.log >&2
  exit 1
fi

fail=0

online_field() {
  timeout 5 ros2 topic echo --once /sentry/online_info 2>/dev/null | \
    awk -v k="$1:" '$1==k{print $2; exit}'
}

game_field() {
  timeout 5 ros2 topic echo --once /sentry/game_info 2>/dev/null | \
    awk -v k="$1:" '$1==k{print $2; exit}'
}

call_stage() {
  timeout 5 ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
    "{stage: $1}" >/dev/null 2>&1 || true
}

set_world() {
  timeout 5 ros2 service call /sentry_sim/set_world sentry_decision_msgs/srv/SetWorld \
    "{field: '$1', value: $2}" >/tmp/effects_setworld.log 2>&1 || true
}

# 补给区免费发弹量（规则 5.3.2）：比赛已进行 60s 时，在补给区应一次 +100 发（与金币无关）。
# 此处不激活 MatchStageController，否则它会用自身计时覆盖 game_time_remaining。
set_world coins 0.0
set_world self_ammo 0.0
set_world game_status 4.0
set_world game_time_remaining 360.0
sleep 2
supply_ammo="$(online_field bullets_remaining)"
if [[ -z "${supply_ammo}" || "${supply_ammo}" -lt 100 ]]; then
  echo "FAIL: 补给区免费发弹量未发放（期望 >=100，实际 ${supply_ammo}）" >&2
  fail=1
fi
if ! grep -q "补给区免费发弹量" /tmp/effects_sim.log; then
  echo "FAIL: 仿真日志缺少补给区免费发弹量记录" >&2
  fail=1
fi

# 进入比赛中。
call_stage 0
sleep 0.5
call_stage 4
sleep 0.5

# 兑换发弹：金币 200、发弹量保持 100（高于低弹阈值，避免任务树也发起兑换）。
set_world coins 200.0
set_world self_ammo 100.0
sleep 1
ammo0="$(online_field bullets_remaining)"
coin0="$(game_field coin_remaining)"

# 注入人工资源请求（每次注入都应视为一次新的兑换）。
inject_exchange() {
  timeout 5 ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
    "{command: 'set_intent', args: \"{field: resource_request, value: {ammo: 50, hp: 0, revive: false}, lease_sec: 0, reason: 'smoke'}\"}" \
    >/dev/null 2>&1 || true
}

inject_exchange
sleep 2

ammo1="$(online_field bullets_remaining)"
coin1="$(game_field coin_remaining)"
if [[ -z "${ammo1}" || "${ammo1}" -le "${ammo0}" ]]; then
  echo "FAIL: 兑换发弹后发弹量未增加（${ammo0} -> ${ammo1}）" >&2
  fail=1
fi
if [[ -z "${coin1}" || $((coin0 - coin1)) -ne 50 ]]; then
  echo "FAIL: 兑换 50 发应扣 50 金币（${coin0} -> ${coin1}）" >&2
  fail=1
fi

# 再次注入同值请求：one-shot 记忆应被清除，可再次兑换。
inject_exchange
sleep 2
ammo2="$(online_field bullets_remaining)"
coin2="$(game_field coin_remaining)"
if [[ -z "${ammo2}" || $((ammo2 - ammo1)) -ne 50 ]]; then
  echo "FAIL: 重复兑换应再次增加 50 发（${ammo1} -> ${ammo2}）" >&2
  fail=1
fi
if [[ -z "${coin2}" || $((coin1 - coin2)) -ne 50 ]]; then
  echo "FAIL: 重复兑换应再扣 50 金币（${coin1} -> ${coin2}）" >&2
  fail=1
fi

# 远程兑换延迟（规则 5.3.2）：确认即扣金币，发弹量 6s 后才到账。
# 关闭 resource 模块，避免决策树在低弹量时自行发起本地兑换干扰观测。
timeout 5 ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_module', args: '{module: resource, enabled: false}'}" >/dev/null 2>&1 || true
set_world coins 500.0
set_world self_ammo 100.0
set_world sentry_info_2 1.0
sleep 0.5
remote_before="$(online_field bullets_remaining)"
timeout 5 ros2 topic pub --once /sentry/decision_command sentry_interfaces/msg/DecisionCommand \
  "{request_id: 9001, action: {kind: 5, mode: 0, interval_ms: 0, value: 1}}" >/dev/null 2>&1 || true
sleep 2
remote_mid="$(online_field bullets_remaining)"
if [[ -z "${remote_mid}" || "${remote_mid}" -ne "${remote_before}" ]]; then
  echo "FAIL: 远程兑换不应立即生效（${remote_before} -> ${remote_mid}）" >&2
  fail=1
fi
sleep 5
remote_after="$(online_field bullets_remaining)"
if [[ -z "${remote_after}" || $((remote_after - remote_before)) -ne 100 ]]; then
  echo "FAIL: 远程兑换应在 6s 后 +100 发（${remote_before} -> ${remote_after}）" >&2
  fail=1
fi
if ! grep -q "远程兑换发弹量生效" /tmp/effects_sim.log; then
  echo "FAIL: 仿真日志缺少远程兑换生效记录" >&2
  fail=1
fi

# 补给区回血：血量设为 100 后应在 2s 内回升。
set_world self_hp 100.0
sleep 0.3
hp0="$(online_field self_health)"
sleep 2
hp1="$(online_field self_health)"
if [[ -z "${hp1}" || "${hp1}" -le "${hp0}" ]]; then
  echo "FAIL: 补给区未回血（${hp0} -> ${hp1}）" >&2
  fail=1
fi

if ! grep -q "补给区回血" /tmp/effects_sim.log; then
  echo "FAIL: 仿真日志缺少补给区回血记录" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "sim effects smoke test passed"
fi
exit "${fail}"
