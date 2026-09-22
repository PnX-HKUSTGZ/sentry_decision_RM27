#!/usr/bin/env bash
# 容器内场景冒烟：启动 decision_node，再用 referee_sim_node 跑一个场景时间轴并断言。
set -euo pipefail

# 使用独立 ROS_DOMAIN_ID，避免与同批次其它集成测试的节点互相干扰。
export ROS_DOMAIN_ID="${SCENARIO_DOMAIN_ID:-41}"

SCENARIO="${1:-}"
if [[ -z "${SCENARIO}" ]]; then
  echo "用法: scenario_smoke_test.sh <scenario.yaml>" >&2
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

DECISION_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/decision_node"
SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"

"${DECISION_BIN}" --rate 20 >/tmp/scenario_decision.log 2>&1 &
DECISION_PID=$!
trap 'kill "${DECISION_PID}" 2>/dev/null || true' EXIT
sleep 2

set +e
"${SIM_BIN}" --scenario "${SCENARIO}" --rate 20 >/tmp/scenario_sim.log 2>&1
rc=$?
set -e

if [[ "${rc}" -ne 0 ]]; then
  echo "场景失败，referee_sim_node 日志:" >&2
  cat /tmp/scenario_sim.log >&2
  echo "decision_node 日志（尾部）:" >&2
  tail -40 /tmp/scenario_decision.log >&2
  exit "${rc}"
fi

echo "scenario smoke test passed"
