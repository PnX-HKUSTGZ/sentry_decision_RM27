#!/usr/bin/env bash
# 容器内冒烟：裁判仿真的比赛阶段服务——前进可快进、回退被拒、重置回未开始。
set -euo pipefail

# 使用独立 ROS_DOMAIN_ID，避免与同批次其它集成测试的节点互相干扰。
export ROS_DOMAIN_ID="${MATCH_DOMAIN_ID:-45}"

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

"${SIM_BIN}" --scenario "${SCENARIO}" --hold >/tmp/match_sim.log 2>&1 &
SP=$!
trap 'kill "${SP}" 2>/dev/null || true' EXIT
sleep 3

fail=0

stage_now() {
  timeout 5 ros2 topic echo --once /sentry/game_info 2>/dev/null | awk '/game_status:/{print $2; exit}'
}

odom_pose() {
  timeout 5 ros2 topic echo --once /aft_mapped_to_init 2>/dev/null | \
    awk '/position:/{p=1;next} p&&/x:/{x=$2} p&&/y:/{print x" "$2; exit}'
}

expect_stage() {
  local want="$1" label="$2" got
  got="$(stage_now)"
  if [[ "${got}" != "${want}" ]]; then
    echo "FAIL: ${label}（期望 ${want}，实际 ${got}）" >&2
    fail=1
  fi
}

call_stage() {
  ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
    "{stage: $1}" >/tmp/match_call.log 2>&1 || true
}

call_stage 0
sleep 0.5
expect_stage 0 "重置到未开始"

call_stage 2
sleep 0.5
expect_stage 2 "快进到自检"

# 回退：请求 1（<= 当前 2）应被拒绝。
call_stage 1
if ! grep -q "success=False" /tmp/match_call.log; then
  echo "FAIL: 回退请求应被拒绝" >&2
  cat /tmp/match_call.log >&2
  fail=1
fi

call_stage 4
sleep 0.5
expect_stage 4 "快进到比赛中"

call_stage 0
sleep 0.5
expect_stage 0 "重置回未开始"

# 重置位置：demo 的 start_pose 为 home (-11.47, -4.40)。
call_stage 0
sleep 0.5
pose="$(odom_pose)"
if ! python3 -c "import sys; x, y = map(float, sys.argv[1].split()); sys.exit(0 if abs(x + 11.47) < 0.05 and abs(y + 4.40) < 0.05 else 1)" "${pose}"; then
  echo "FAIL: 重置后位姿应为 (-11.47, -4.40)，实际 ${pose}" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "match smoke test passed"
fi
exit "${fail}"
