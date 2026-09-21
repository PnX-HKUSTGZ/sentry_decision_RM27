#!/usr/bin/env bash
# 录制一局场景 bag，再用 replay_main 离线重放，检查干预在原时刻复现。
set -euo pipefail

SCENARIO="${1:-}"
if [[ -z "${SCENARIO}" ]]; then
  echo "用法: replay_smoke_test.sh <scenario.yaml>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY="${SCENARIO_OVERLAY_DIR:-${ROOT}/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${OVERLAY}/setup.bash" ]]; then
  source "${OVERLAY}/setup.bash"
fi
set -u

# 独立 ROS domain，避免与并行的集成测试互相干扰。
export ROS_DOMAIN_ID="${REPLAY_DOMAIN_ID:-43}"

DECISION_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/decision_node"
REPLAY_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/replay_main"
SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"
BAG=/tmp/sentry_replay_smoke_bag

rm -rf "${BAG}"
# 先起录制再起节点，保证从头录到上行与干预。
ros2 bag record -o "${BAG}" \
  /sentry/game_info /sentry/online_info /sentry/offline_info /sentry/team_info /sentry/radar_info \
  /aft_mapped_to_init /decision/intervention >/tmp/replay_record.log 2>&1 & REC=$!
sleep 1
"${DECISION_BIN}" --ticks 0 >/tmp/replay_decision.log 2>&1 & DP=$!
trap 'kill "${REC}" "${DP}" 2>/dev/null || true' EXIT
sleep 2

# 跑完整场景（含 add_intent / disable），结束后停止录制。
"${SIM_BIN}" --scenario "${SCENARIO}" --rate 20 >/tmp/replay_sim.log 2>&1
sleep 1
kill "${REC}" "${DP}" 2>/dev/null || true
wait "${REC}" "${DP}" 2>/dev/null || true
trap - EXIT

# 离线重放。
if ! "${REPLAY_BIN}" --bag "${BAG}" >/tmp/replay_main.log 2>&1; then
  echo "replay_main 失败:" >&2
  cat /tmp/replay_main.log >&2
  exit 1
fi

fail=0
check() {
  if ! grep -q "$1" /tmp/replay_main.log; then
    echo "FAIL: $2" >&2
    fail=1
  fi
}
check '载入 bag' '未载入 bag'
check '注入干预' '未重放人工干预'
check '回放结束' '回放未结束'

if [[ "${fail}" -ne 0 ]]; then
  cat /tmp/replay_main.log >&2
  exit 1
fi
echo "replay smoke test passed"
