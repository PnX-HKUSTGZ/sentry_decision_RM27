#!/usr/bin/env bash
# 容器内冒烟：裁判系统按前置条件拒绝非法动作并打印日志。
# 直接向下位机侧发 DecisionCommand，验证执行端校验（不经过决策内部干预）。
#   - 本地兑换发弹量：不在增益点 -> 拒绝
#   - 兑换血量：未脱战 -> 拒绝；脱战 -> 允许
set -euo pipefail

export ROS_DOMAIN_ID="${REFEREE_GUARD_DOMAIN_ID:-47}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY="${SCENARIO_OVERLAY_DIR:-${ROOT}/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${OVERLAY}/setup.bash" ]]; then
  source "${OVERLAY}/setup.bash"
fi
set -u

SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"
SCENARIO="$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml"

# 补给区挪到远处，机器人（demo 起点）不在任何增益点，便于验证非法兑换被拒。
"${SIM_BIN}" --scenario "${SCENARIO}" --hold \
  --ros-args -p supply_center_x:=100.0 -p supply_center_y:=100.0 -p supply_radius:=1.0 \
  -p base_buff_center_x:=100.0 -p base_buff_center_y:=100.0 -p base_buff_radius:=1.0 \
  -p our_outpost_center_x:=100.0 -p our_outpost_center_y:=100.0 -p our_outpost_radius:=1.0 \
  >/tmp/guard_sim.log 2>&1 & SP=$!
trap 'kill "${SP}" 2>/dev/null || true' EXIT
sleep 3

fail=0
check_log() {
  if ! grep -q "$1" /tmp/guard_sim.log; then
    echo "FAIL: 仿真日志缺少「$1」" >&2
    fail=1
  fi
}

online_field() {
  timeout 5 ros2 topic echo --once /sentry/online_info 2>/dev/null | \
    awk -v k="$1:" '$1==k{print $2; exit}'
}

set_world() {
  timeout 5 ros2 service call /sentry_sim/set_world sentry_decision_msgs/srv/SetWorld \
    "{field: '$1', value: $2}" >/dev/null 2>&1 || true
}

# 直接发动作：kind 1 = 本地兑换发弹，kind 2 = 兑换血量（需脱战）。
request_seq=8100
send_action() {
  request_seq=$((request_seq + 1))
  timeout 5 ros2 topic pub --once /sentry/decision_command sentry_interfaces/msg/DecisionCommand \
    "{request_id: ${request_seq}, action: {kind: $1, mode: 0, interval_ms: 0, value: $2}}" \
    >/dev/null 2>&1 || true
}

call_stage() {
  timeout 5 ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
    "{stage: $1}" >/dev/null 2>&1 || true
}

call_stage 0
sleep 0.5
call_stage 4
sleep 0.5

# 1) 本地兑换发弹量：不在增益点 -> 拒绝，弹量不变。
set_world self_ammo 10.0
sleep 0.5
send_action 1 50
sleep 2
ammo_after="$(online_field bullets_remaining)"
if [[ "${ammo_after}" != "10" ]]; then
  echo "FAIL: 非法兑换不应改变发弹量（期望 10，实际 ${ammo_after}）" >&2
  fail=1
fi
check_log "未占领可兑换增益点"

# 2) 兑换血量：未脱战 -> 拒绝。
set_world disengaged 0.0
set_world self_hp 100.0
sleep 0.5
send_action 2 50
sleep 2
check_log "未脱战"

# 3) 兑换血量：脱战 -> 允许，血量上升。
set_world disengaged 1.0
set_world self_hp 100.0
sleep 0.5
send_action 2 50
sleep 2
hp_after="$(online_field self_health)"
if [[ -z "${hp_after}" || "${hp_after}" -le 100 ]]; then
  echo "FAIL: 脱战后兑换血量应生效（实际 ${hp_after}）" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "referee guard smoke test passed"
fi
exit "${fail}"
