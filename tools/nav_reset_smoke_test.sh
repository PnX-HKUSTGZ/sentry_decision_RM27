#!/usr/bin/env bash
# 容器内冒烟：导航成功 -> 重置 -> 重新开始，仍能再次进入 Navigate/Outpost。
set -euo pipefail

export ROS_DOMAIN_ID="${NAV_RESET_DOMAIN_ID:-47}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY="${SCENARIO_OVERLAY_DIR:-${ROOT}/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${OVERLAY}/setup.bash" ]]; then
  source "${OVERLAY}/setup.bash"
fi
set -u

DECISION_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/task_node"
SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"
SCENARIO="$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/nav_reset.yaml"

"${DECISION_BIN}" --rate 20 >/tmp/nav_reset_decision.log 2>&1 &
DP=$!
"${SIM_BIN}" --scenario "${SCENARIO}" --hold >/tmp/nav_reset_sim.log 2>&1 &
SP=$!
trap 'kill "${DP}" "${SP}" 2>/dev/null || true' EXIT

state_json() {
  timeout 5 ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
    "{command: 'list_state'}" 2>/dev/null | sed -n "s/.*state_json='\(.*\)')/\1/p"
}

call_stage() {
  timeout 5 ros2 service call /sentry_sim/set_game_stage sentry_decision_msgs/srv/SetGameStage \
    "{stage: $1}" >/dev/null 2>&1 || true
}

fail=0
reached_outpost=0
for _ in $(seq 1 25); do
  if state_json | grep -q '"id":"Outpost"'; then
    reached_outpost=1
    break
  fi
  sleep 1
done
if [[ "${reached_outpost}" -ne 1 ]]; then
  echo "FAIL: 未在 25s 内到达 Outpost" >&2
  tail -30 /tmp/nav_reset_decision.log >&2 || true
  fail=1
fi

call_stage 0
sleep 1
call_stage 3
sleep 7

json="$(state_json)"
task_state="$(printf '%s' "${json}" | sed -n 's/.*"task_state":\([0-9][0-9]*\).*/\1/p')"
if [[ -z "${task_state}" || "${task_state}" -eq 0 ]]; then
  echo "FAIL: 重置并重新开始后仍为 idle（task_state=${task_state}）" >&2
  printf '%s\n' "${json}" >&2
  fail=1
fi

if grep -q "导航已结束但不在到达半径内" /tmp/nav_reset_decision.log; then
  echo "FAIL: 重置后仍出现旧 nav.reached 导致的导航失败" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "nav reset smoke test passed"
fi
exit "${fail}"
