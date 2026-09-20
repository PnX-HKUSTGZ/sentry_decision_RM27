#!/usr/bin/env bash
# 容器内冒烟测试 sentry_decision_io：启动节点，检查上下行话题与导航 action client 是否建立。
set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/ws/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${INSTALL_DIR}/setup.bash" ]]; then
  source "${INSTALL_DIR}/setup.bash"
fi
set -u

NODE_BIN="$(ros2 pkg prefix sentry_decision_io)/lib/sentry_decision_io/io_node"
"${NODE_BIN}" >/tmp/io_node.log 2>&1 &
NODE_PID=$!
trap 'kill "${NODE_PID}" 2>/dev/null || true' EXIT
sleep 2

INFO="$(ros2 node info /sentry_decision_io)"
echo "${INFO}"

fail=0
for topic in /sentry/game_info /sentry/online_info /sentry/offline_info /sentry/team_info /sentry/radar_info /sentry/decision_ack /aft_mapped_to_init; do
  if ! grep -q "${topic}" <<<"${INFO}"; then
    echo "缺少订阅: ${topic}" >&2
    fail=1
  fi
done

for topic in /sentry/decision_command /cmd_vel; do
  if ! grep -q "${topic}" <<<"${INFO}"; then
    echo "缺少发布: ${topic}" >&2
    fail=1
  fi
done

if ! grep -q "navigate_to_pose" <<<"${INFO}"; then
  echo "缺少导航 action client: navigate_to_pose" >&2
  fail=1
fi

if [[ "${fail}" -eq 0 ]]; then
  echo "io smoke test passed"
fi
exit "${fail}"
