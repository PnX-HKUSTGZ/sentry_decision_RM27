#!/usr/bin/env bash
# 从本地导航仓库重建双仓库联调镜像。导航仓库源码更新后运行一次即可，
# 不需要再改 tools/demo.sh。
#
#   tools/build_nav_image.sh
#   NAV_IMAGE=rm27_nav:jazzy NAV_REPO=../sentry_navgation_RM27 tools/build_nav_image.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAV_REPO="${NAV_REPO:-$(cd "${ROOT}/../sentry_navgation_RM27" && pwd)}"
NAV_IMAGE="${NAV_IMAGE:-rm27_nav:jazzy}"
NAV_BASE_IMAGE="${NAV_BASE_IMAGE:-${NAV_IMAGE}}"

if [[ ! -d "${NAV_REPO}/src" ]]; then
  echo "导航仓库不存在或缺少 src/: ${NAV_REPO}" >&2
  exit 2
fi
if ! docker image inspect "${NAV_BASE_IMAGE}" >/dev/null 2>&1; then
  echo "基础镜像不存在: ${NAV_BASE_IMAGE}" >&2
  echo "首次构建请先按导航仓库 README 准备可用的 Jazzy 镜像。" >&2
  exit 2
fi

DOCKERFILE="$(mktemp)"
trap 'rm -f "${DOCKERFILE}"' EXIT
cat > "${DOCKERFILE}" <<'EOF'
ARG NAV_BASE_IMAGE=rm27_nav:jazzy
FROM ${NAV_BASE_IMAGE}
SHELL ["/bin/bash", "-c"]

# 当前导航分支面向 Jazzy，并使用 GTSAM 4.2。
RUN apt-get update && \
    apt-get install -y --no-install-recommends ros-jazzy-gtsam && \
    rm -rf /var/lib/apt/lists/*

RUN rm -rf /ws/src /ws/build /ws/install /ws/log
COPY src/ /ws/src/

RUN source /opt/ros/jazzy/setup.bash && \
    rosdep install --from-paths /ws/src --ignore-src -r -y --rosdistro jazzy && \
    cd /ws && \
    colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
EOF

echo "[nav-build] 从 ${NAV_REPO} 构建 ${NAV_IMAGE}"
DOCKER_BUILDKIT="${DOCKER_BUILDKIT:-0}" docker build \
  --build-arg "NAV_BASE_IMAGE=${NAV_BASE_IMAGE}" \
  -f "${DOCKERFILE}" -t "${NAV_IMAGE}" "${NAV_REPO}"
echo "[nav-build] 完成: ${NAV_IMAGE}"
