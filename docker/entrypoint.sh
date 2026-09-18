#!/usr/bin/env bash
# 容器入口：在挂载的工作区里用 colcon 构建 / 测试。
#
# 用法：
#   entrypoint.sh            # 交互 shell
#   entrypoint.sh build      # 构建（并生成合并的 compile_commands.json）
#   entrypoint.sh test       # 构建并运行测试
set -euo pipefail

WS_DIR="${WS_DIR:-/ws}"
BUILD_DIR="${BUILD_DIR:-${WS_DIR}/.docker-build}"

git config --global --add safe.directory "${WS_DIR}" || true

set +u
source /opt/ros/jazzy/setup.bash
set -u

build() {
  mkdir -p "${BUILD_DIR}"
  colcon build --symlink-install --base-paths src \
    --build-base "${BUILD_DIR}/build" \
    --install-base "${BUILD_DIR}/install" \
    --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
  python3 - "${BUILD_DIR}" <<'PY'
import glob
import json
import os
import sys

build_dir = sys.argv[1]
entries = []
for path in glob.glob(os.path.join(build_dir, "build", "*", "compile_commands.json")):
    with open(path) as handle:
        entries.extend(json.load(handle))
out = os.path.join(build_dir, "compile_commands.json")
with open(out, "w") as handle:
    json.dump(entries, handle, indent=2)
print("[entrypoint] merged compile_commands.json:", out)
PY
}

run_tests() {
  colcon test --base-paths src \
    --build-base "${BUILD_DIR}/build" \
    --install-base "${BUILD_DIR}/install"
  colcon test-result --verbose --test-result-base "${BUILD_DIR}/build"
}

case "${1:-shell}" in
  build) build ;;
  test) build; run_tests ;;
  shell) exec bash ;;
  *) exec "$@" ;;
esac
