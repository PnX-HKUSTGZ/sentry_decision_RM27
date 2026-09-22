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
  >/tmp/effects_sim.log 2>&1 & SP=$!
trap 'kill "${DP}" "${SP}" 2>/dev/null || true' EXIT
sleep 3

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
  ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
    "{stage: $1}" >/dev/null 2>&1 || true
}

set_world() {
  ros2 service call /sentry_sim/set_world sentry_decision_msgs/srv/SetWorld \
    "{field: '$1', value: $2}" >/tmp/effects_setworld.log 2>&1 || true
}

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

ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_intent', args: \"{field: resource_request, value: {ammo: 50, hp: 0, revive: false}, lease_sec: 0, reason: 'smoke'}\"}" \
  >/dev/null 2>&1 || true
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
