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

time_now() {
  timeout 5 ros2 topic echo --once /sentry/game_info 2>/dev/null | \
    awk '/game_time_remaining:/{print $2; exit}'
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

# 暂停：冻结倒计时（剩余时间在 2s 内不应变化）。
call_stage 3
sleep 0.5
ros2 service call /sentry_sim/set_game_pause sentry_decision_msgs/srv/SetGamePause \
  "{paused: true}" >/tmp/match_pause.log 2>&1 || true
if ! grep -q "paused=True" /tmp/match_pause.log; then
  echo "FAIL: 暂停应返回 paused=True" >&2
  cat /tmp/match_pause.log >&2
  fail=1
fi
paused_t0="$(time_now)"
sleep 2
paused_t1="$(time_now)"
if [[ "${paused_t0}" != "${paused_t1}" ]]; then
  echo "FAIL: 暂停期间剩余时间不应变化（${paused_t0} -> ${paused_t1}）" >&2
  fail=1
fi
ros2 service call /sentry_sim/set_game_pause sentry_decision_msgs/srv/SetGamePause \
  "{paused: false}" >/dev/null 2>&1 || true

# 重置位置：demo 的 start_pose 为 (-5, 3)。
call_stage 0
sleep 0.5
pose="$(odom_pose)"
if [[ "${pose}" != "-5.0 3.0" ]]; then
  echo "FAIL: 重置后位姿应为 (-5.0, 3.0)，实际 ${pose}" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "match smoke test passed"
fi
exit "${fail}"
