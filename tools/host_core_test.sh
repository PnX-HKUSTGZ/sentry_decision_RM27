#!/usr/bin/env bash
# 在无 ROS 的宿主机上编译并运行 core 与 sim 的纯逻辑单元测试。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/src/sentry_decision_core"
SIM="$ROOT/src/sentry_decision_sim"
OUT="$(mktemp -d)"

CORE_SOURCES=("$CORE/src/arbiter.cpp" "$CORE/src/logging.cpp" "$CORE/src/referee_protocol.cpp"
  "$CORE/src/replay.cpp" "$CORE/src/world_model.cpp")
SIM_SOURCES=("$SIM/src/referee_simulator.cpp" "$SIM/src/nav_simulator.cpp")

for test in test_arbiter test_logging test_world_model test_referee_protocol test_replay; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" "${CORE_SOURCES[@]}" \
    "$CORE/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done

for test in test_referee_simulator test_nav_simulator; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" -I"$SIM/include" \
    "${CORE_SOURCES[@]}" "${SIM_SOURCES[@]}" "$SIM/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done
