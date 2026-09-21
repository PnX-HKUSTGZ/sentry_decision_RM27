#!/usr/bin/env bash
# 容器内冒烟：启动决策 + 仿真 + rosbridge + 静态服务，检查面板资源与决策话题可用。
# 未安装 rosbridge_server 时跳过（不视为失败）。
# 使用独立 ROS_DOMAIN_ID，避免与同批次其它集成测试的节点互相干扰。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY="${SCENARIO_OVERLAY_DIR:-${ROOT}/.docker-build/install}"

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f "${OVERLAY}/setup.bash" ]]; then
  source "${OVERLAY}/setup.bash"
fi
set -u

export ROS_DOMAIN_ID="${VIZ_DOMAIN_ID:-42}"

if ! ros2 pkg executables rosbridge_server 2>/dev/null | grep -q rosbridge_websocket; then
  echo "skip viz smoke: rosbridge_server 未安装"
  exit 0
fi

DECISION_BIN="$(ros2 pkg prefix sentry_decision_bringup)/lib/sentry_decision_bringup/decision_node"
SIM_BIN="$(ros2 pkg prefix sentry_decision_sim)/lib/sentry_decision_sim/referee_sim_node"
WEB_DIR="$(ros2 pkg prefix sentry_decision_viz)/share/sentry_decision_viz/web"

"${DECISION_BIN}" --ticks 0 >/tmp/viz_decision.log 2>&1 & DP=$!
"${SIM_BIN}" --rate 20 >/tmp/viz_sim.log 2>&1 & SP=$!
ros2 run rosbridge_server rosbridge_websocket --ros-args -p port:=9090 >/tmp/viz_rosbridge.log 2>&1 & RP=$!
python3 -m http.server 8080 --directory "${WEB_DIR}" >/tmp/viz_http.log 2>&1 & HP=$!
trap 'kill "${DP}" "${SP}" "${RP}" "${HP}" 2>/dev/null || true' EXIT
sleep 4

fail=0
check() {
  # $1 期望，$2 实际，$3 说明
  if [[ "$2" != "$1" ]]; then
    echo "FAIL: $3（期望 $1，实际 $2）" >&2
    fail=1
  fi
}

http_status() {
  python3 -c "import urllib.request,sys; print(urllib.request.urlopen(sys.argv[1], timeout=5).status)" "$1" 2>/dev/null || echo ERR
}

tcp_ok() {
  python3 -c "import socket,sys; s=socket.create_connection(('127.0.0.1', int(sys.argv[1])), 5); s.close(); print('200')" "$1" 2>/dev/null || echo ERR
}

wait_echo() {
  # 等待话题出现并拿到一帧，最多约 15s。
  local topic="$1"
  for _ in $(seq 1 15); do
    if timeout 3 ros2 topic echo "${topic}" --once >/tmp/viz_echo.log 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

for path in / /index.html /style.css /src/main.js /vendor/roslib.min.js; do
  check "200" "$(http_status "http://127.0.0.1:8080${path}")" "静态资源 ${path}"
done

check "200" "$(tcp_ok 9090)" "rosbridge 端口"

for topic in /decision/tree_status /decision/world_state /decision/state; do
  if ! wait_echo "${topic}"; then
    echo "FAIL: 话题 ${topic} 无数据" >&2
    fail=1
  fi
done

ros2 launch sentry_decision_viz viz.launch.py --show-args >/tmp/viz_launch_args.log 2>&1 || fail=1
check "1" "$(grep -c 'rosbridge_port' /tmp/viz_launch_args.log || true)" "viz.launch.py 可解析"

if [[ "${fail}" -eq 0 ]]; then
  echo "viz smoke test passed"
fi
exit "${fail}"
