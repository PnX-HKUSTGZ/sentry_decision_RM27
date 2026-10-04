#!/usr/bin/env bash
# 一键启动仿真演示：网页面板 + 裁判仿真 + 决策节点。
#
# 宿主运行会自动起一个映射 8080/9090 的容器；容器内运行则直接启动。
# 三个进程的 stdout 汇到当前终端并按 [viz]/[sim]/[decision] 加前缀，
# 同时各写一份到 .docker-build/demo_logs/<name>.log。Ctrl+C 停止全部。
#
# 用法：
#   tools/demo.sh                    # 面板 + 仿真 + 决策（独立仿真）
#   tools/demo.sh --no-viz           # 只起仿真 + 决策
#   tools/demo.sh --with-nav         # 双仓库联调：额外起真实导航容器
#   tools/demo.sh --with-nav --nav-silent  # 导航不开 RViz
#   tools/demo.sh --build            # 先构建工作区
#   tools/demo.sh --scenario <path>  # 指定场景（容器内路径）
#   tools/demo.sh --rate 20          # 仿真频率
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE="${DEMO_IMAGE:-sentry_decision_rm27:jazzy}"
INSTALL="${ROOT}/.docker-build/install"
NETWORK="${DEMO_NAV_NETWORK:-rm27net}"
NAV_CONTAINER="${DEMO_NAV_CONTAINER:-rm27_nav_sim}"

# 宿主：按是否要面板决定端口映射，按是否双仓库决定导航容器，再把参数交给容器内的自己。
if [[ ! -f /.dockerenv ]]; then
  need_viz=1
  need_build=0
  with_nav=0
  nav_silent=0
  for arg in "$@"; do
    case "${arg}" in
      --no-viz) need_viz=0 ;;
      --build) need_build=1 ;;
      --with-nav) with_nav=1 ;;
      --nav-silent) nav_silent=1 ;;
    esac
  done
  [[ -f "${INSTALL}/setup.bash" ]] || need_build=1
  tty=(-i)
  if [[ -t 0 ]]; then tty+=(-t); fi
  ports=()
  if [[ "${need_viz}" == 1 ]]; then
    ports=(-p 8080:8080 -p 9090:9090)
  fi
  user_args=()
  if [[ "${need_build}" == 1 ]]; then user_args=(--user root); fi

  if [[ "${with_nav}" == 1 ]]; then
    NAV_REPO="${NAV_REPO:-$(cd "${ROOT}/../sentry_navgation_RM27" && pwd)}"
    NAV_IMAGE="${NAV_IMAGE:-rm27_nav:jazzy}"
    docker network inspect "${NETWORK}" >/dev/null 2>&1 || docker network create "${NETWORK}" >/dev/null
    docker rm -f "${NAV_CONTAINER}" >/dev/null 2>&1 || true
    nav_use_rviz=true
    nav_display=()
    if [[ "${nav_silent}" == 1 ]]; then
      nav_use_rviz=false
    elif [[ -z "${DISPLAY:-}" || -z "$(ls -A /tmp/.X11-unix 2>/dev/null)" ]]; then
      # RViz 连不上显示会退出，而它的死亡会触发 Gazebo 整套 launch 关闭。
      echo "[demo] 未检测到可用 X11（DISPLAY 或 /tmp/.X11-unix），自动关闭导航 RViz"
      nav_use_rviz=false
    else
      nav_display=(-e "DISPLAY=${DISPLAY:-}" -v /tmp/.X11-unix:/tmp/.X11-unix:rw)
    fi
    docker run -d --name "${NAV_CONTAINER}" --network "${NETWORK}" "${nav_display[@]}" \
      -v "${NAV_REPO}/src/pb2025_nav_bringup/pcd/simulation":/pcd:ro \
      "${NAV_IMAGE}" bash -lc "source /opt/ros/jazzy/setup.bash && source /ws/install/setup.bash && exec ros2 launch rm_27_stimulation sim_with_nav.launch.py world:=RMUC2026 slam:=False gui:=false use_rviz:=${nav_use_rviz} prior_pcd_file:=/pcd/RMUC2026.pcd"
    echo "[demo] 导航容器 ${NAV_CONTAINER} 已启动，等待 /navigate_to_pose…"
    cleanup_nav() { docker rm -f "${NAV_CONTAINER}" >/dev/null 2>&1 || true; }
    trap cleanup_nav EXIT INT TERM
    nav_ok=0
    for _ in $(seq 1 60); do
      if ! docker ps --format '{{.Names}}' | grep -qx "${NAV_CONTAINER}"; then
        echo "[demo] 导航容器已退出，日志尾部：" >&2
        docker logs "${NAV_CONTAINER}" 2>&1 | tail -25 >&2
        exit 1
      fi
      if docker exec "${NAV_CONTAINER}" bash -lc "source /opt/ros/jazzy/setup.bash && source /ws/install/setup.bash && ros2 action list 2>/dev/null | grep -q '^/navigate_to_pose\$'"; then
        nav_ok=1
        break
      fi
      sleep 2
    done
    if [[ "${nav_ok}" != 1 ]]; then
      echo "[demo] 等待 /navigate_to_pose 超时，导航容器日志尾部：" >&2
      docker logs "${NAV_CONTAINER}" 2>&1 | tail -15 >&2
      exit 1
    fi
    echo "[demo] 导航就绪，启动决策容器"
    set +e
    docker run --rm "${tty[@]}" "${user_args[@]}" "${ports[@]}" --network "${NETWORK}" \
      -v "${ROOT}":/ws -w /ws \
      --entrypoint /ws/docker/entrypoint.sh \
      "${IMAGE}" demo "$@"
    status=$?
    exit "${status}"
  fi

  exec docker run --rm "${tty[@]}" "${user_args[@]}" "${ports[@]}" \
    -v "${ROOT}":/ws -w /ws \
    --entrypoint /ws/docker/entrypoint.sh \
    "${IMAGE}" demo "$@"
fi

# ---- 以下在容器内执行 ----
with_viz=1
do_build=0
with_nav=0
scenario=""
rate="20.0"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-viz) with_viz=0; shift ;;
    --with-nav) with_nav=1; shift ;;
    --nav-silent) shift ;;
    --build) do_build=1; shift ;;
    --scenario) scenario="$2"; shift 2 ;;
    --rate) rate="$2"; shift 2 ;;
    -h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
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

sim_args=()
decision_args=()
if [[ "${with_nav}" == 1 ]]; then
  echo "[demo] 双仓库联调：等待导航 /navigate_to_pose 就绪…"
  nav_ready=0
  for _ in $(seq 1 60); do
    if ros2 action list 2>/dev/null | grep -q '^/navigate_to_pose$'; then
      nav_ready=1
      break
    fi
    sleep 2
  done
  if [[ "${nav_ready}" != 1 ]]; then
    echo "[demo] 未等到 /navigate_to_pose：请确认导航容器已启动" >&2
    exit 1
  fi
  # 双仓库：裁判仿真不提供伪导航，位姿取决策回传的 map 系 /decision/world_state；
  # 决策订阅真实导航的 /odometry（odom 系）并用 TF map->odom 转成 map 系。
  sim_args=(--ros-args -p provide_nav:=false)
  decision_args=(--ros-args -p odom_topic:=/odometry -p odom_frame:=odom)
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
  ( "$@" 2>&1 | stdbuf -oL sed -u "s/^/[${name}] /" | stdbuf -oL tee "${log}" ) &
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
start sim ros2 run sentry_decision_sim referee_sim_node --scenario "${scenario}" --hold --rate "${rate}" "${sim_args[@]}"
start decision ros2 run sentry_decision_bringup decision_node "${decision_args[@]}"

if [[ "${with_nav}" == 1 ]]; then
  echo "[demo] 双仓库联调模式：导航容器提供 /navigate_to_pose，位姿经 map->odom 变换"
fi
echo "[demo] 面板 http://localhost:8080（rosbridge ws://localhost:9090）；--no-viz 可关闭面板"
echo "[demo] Ctrl+C 停止全部"

# 任一进程退出即触发 cleanup。
wait
