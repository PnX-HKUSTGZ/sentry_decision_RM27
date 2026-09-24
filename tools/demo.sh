#!/usr/bin/env bash
# 一键启动仿真演示：网页面板 + 裁判仿真 + 决策节点。
#
# 宿主运行会自动起一个映射 8080/9090 的容器；容器内运行则直接启动。
# 三个进程的 stdout 汇到当前终端并按 [viz]/[sim]/[decision] 加前缀，
# 同时各写一份到 .docker-build/demo_logs/<name>.log。Ctrl+C 停止全部。
#
# 用法：
#   tools/demo.sh                    # 面板 + 仿真 + 决策
#   tools/demo.sh --no-viz           # 只起仿真 + 决策
#   tools/demo.sh --build            # 先构建工作区
#   tools/demo.sh --scenario <path>  # 指定场景（容器内路径）
#   tools/demo.sh --rate 20          # 仿真频率
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE="${DEMO_IMAGE:-sentry_decision_rm27:jazzy}"
INSTALL="${ROOT}/.docker-build/install"

# 宿主：按是否要面板决定端口映射，再把参数原样交给容器内的自己。
if [[ ! -f /.dockerenv ]]; then
  need_viz=1
  need_build=0
  for arg in "$@"; do
    case "${arg}" in
      --no-viz) need_viz=0 ;;
      --build) need_build=1 ;;
    esac
  done
  [[ -f "${INSTALL}/setup.bash" ]] || need_build=1
  tty=(-i)
  if [[ -t 0 ]]; then tty+=(-t); fi
  ports=()
  if [[ "${need_viz}" == 1 ]]; then
    ports=(-p 8080:8080 -p 9090:9090)
  fi
  # 构建会写 .docker-build/install，按既有约定用 root，避免 INSTALL 权限报错。
  user_args=()
  if [[ "${need_build}" == 1 ]]; then user_args=(--user root); fi
  exec docker run --rm "${tty[@]}" "${user_args[@]}" "${ports[@]}" \
    -v "${ROOT}":/ws -w /ws \
    --entrypoint /ws/docker/entrypoint.sh \
    "${IMAGE}" demo "$@"
fi

# ---- 以下在容器内执行 ----
with_viz=1
do_build=0
scenario=""
rate="20.0"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-viz) with_viz=0; shift ;;
    --build) do_build=1; shift ;;
    --scenario) scenario="$2"; shift 2 ;;
    --rate) rate="$2"; shift 2 ;;
    -h|--help) sed -n '2,13p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "未知参数: $1" >&2; exit 2 ;;
  esac
done

# ROS 的 setup.bash 会引用未定义变量，source 期间临时关掉 -u。
set +u
source /opt/ros/jazzy/setup.bash
set -u
if [[ "${do_build}" == 1 || ! -f "${INSTALL}/setup.bash" ]]; then
  echo "[demo] 构建工作区（colcon build）…"
  "${ROOT}/docker/entrypoint.sh" build
fi
set +u
source "${INSTALL}/setup.bash"
set -u

if [[ -z "${scenario}" ]]; then
  scenario="$(ros2 pkg prefix sentry_decision_sim)/share/sentry_decision_sim/scenario/demo.yaml"
fi

LOG_DIR="${DEMO_LOG_DIR:-${ROOT}/.docker-build/demo_logs}"
mkdir -p "${LOG_DIR}" 2>/dev/null || true
if [[ ! -w "${LOG_DIR}" ]]; then
  # 目录不可写（例如之前用 root 跑过）时退回 /tmp，至少当前会话能看到日志。
  LOG_DIR="/tmp/sentry_demo_logs"
  mkdir -p "${LOG_DIR}"
fi
set -m
declare -a DEMO_PGIDS=()

start() {
  local name="$1"; shift
  local log="${LOG_DIR}/${name}.log"
  : > "${log}"
  # 每个进程自成进程组（set -m），便于整组终止；输出加前缀并同时写日志。
  ( "$@" 2>&1 | stdbuf -oL sed -u "s/^/[$name] /" | stdbuf -oL tee "${log}" ) &
  DEMO_PGIDS+=("$!")
  echo "[demo] 启动 ${name}（日志 ${log}）"
}

stop_all() {
  trap - EXIT INT TERM
  set +e
  local pgid
  for pgid in "${DEMO_PGIDS[@]}"; do
    kill -TERM -- "-${pgid}" 2>/dev/null || true
  done
  sleep 1
  for pgid in "${DEMO_PGIDS[@]}"; do
    kill -KILL -- "-${pgid}" 2>/dev/null || true
  done
  wait 2>/dev/null
  echo "[demo] 已停止"
}
trap 'stop_all' EXIT INT TERM

if [[ "${with_viz}" == 1 ]]; then
  start viz ros2 launch sentry_decision_viz viz.launch.py
fi
start sim ros2 run sentry_decision_sim referee_sim_node --scenario "${scenario}" --hold --rate "${rate}"
start decision ros2 run sentry_decision_bringup decision_node

echo "[demo] 面板 http://localhost:8080（rosbridge ws://localhost:9090）；--no-viz 可关闭面板"
echo "[demo] Ctrl+C 停止全部"

# 任一进程退出即触发 cleanup。
wait
