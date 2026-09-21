#!/usr/bin/env bash
# 容器内冒烟：验证 decision_node 的干预 action / service 与 /decision/intervention。
set -euo pipefail

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

"${DECISION_BIN}" --ticks 0 >/tmp/intervention_decision.log 2>&1 & DP=$!
"${SIM_BIN}" --rate 20 >/tmp/intervention_sim.log 2>&1 & SP=$!
trap 'kill "${DP}" "${SP}" 2>/dev/null || true' EXIT
sleep 3

fail=0
check() {
  if ! grep -q "$1" <<<"$2"; then
    echo "FAIL: $3" >&2
    fail=1
  fi
}

STATE="$(ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'list_state', args: ''}" 2>&1)"
check 'success=True' "${STATE}" "list_state 未成功"
check 'state_json' "${STATE}" "list_state 未返回 state_json"

MODULE="$(ros2 service call /decision/debug sentry_decision_msgs/srv/DebugCommand \
  "{command: 'set_module', args: '{module: nav, enabled: false}'}" 2>&1)"
check 'success=True' "${MODULE}" "set_module 未成功"

# 先订阅，避免错过一次性事件。
(timeout 4 ros2 topic echo /decision/intervention >/tmp/intervention_topic.log 2>&1) & EP=$!
sleep 1
GOAL="$(ros2 action send_goal /decision/manual_override \
  sentry_decision_msgs/action/ManualOverride \
  "{field: 0, value: '[1.0, 2.0]', lease_sec: 2.0, reason: 'manual'}" 2>&1)"
check 'override finished' "${GOAL}" "manual_override 未成功结束"
wait "${EP}" 2>/dev/null || true

EVENT="$(cat /tmp/intervention_topic.log)"
check 'kind:' "${EVENT}" "未收到 /decision/intervention"
check '注入意图' "$(cat /tmp/intervention_decision.log)" "决策日志缺少注入记录"

if [[ "${fail}" -eq 0 ]]; then
  echo "intervention smoke test passed"
fi
exit "${fail}"
