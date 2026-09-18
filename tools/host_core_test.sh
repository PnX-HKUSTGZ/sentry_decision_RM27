#!/usr/bin/env bash
# 在无 ROS 的宿主机上编译并运行 core 的纯逻辑单元测试。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/src/sentry_decision_core"
OUT="$(mktemp -d)"

g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" "$CORE/src/arbiter.cpp" "$CORE/test/test_arbiter.cpp" -o "$OUT/test_arbiter"
"$OUT/test_arbiter"
