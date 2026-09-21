#!/usr/bin/env bash
# 在无 ROS 的宿主机上编译并运行 core 与 sim 的纯逻辑单元测试。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/src/sentry_decision_core"
SIM="$ROOT/src/sentry_decision_sim"
OUT="$(mktemp -d)"

NODES="$ROOT/src/sentry_decision_nodes"
CORE_SOURCES=("$CORE/src/action_dispatcher.cpp" "$CORE/src/arbiter.cpp" "$CORE/src/intervention.cpp"
  "$CORE/src/logging.cpp" "$CORE/src/nav_goal_tracker.cpp" "$CORE/src/referee_protocol.cpp"
  "$CORE/src/replay.cpp" "$CORE/src/safety_supervisor.cpp" "$CORE/src/world_model.cpp")
SIM_SOURCES=("$SIM/src/referee_simulator.cpp" "$SIM/src/nav_simulator.cpp"
  "$SIM/src/decision_actuator_sim.cpp")

for test in test_arbiter test_logging test_world_model test_referee_protocol test_replay test_config test_action_dispatcher test_safety_supervisor test_intervention test_nav_goal_tracker; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" "${CORE_SOURCES[@]}" \
    "$CORE/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done

# 战略策略为纯逻辑（不依赖 BT），一并做宿主测试。
g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" -I"$NODES/include" \
  "$NODES/src/rule_based_strategic_policy.cpp" "$NODES/test/test_strategic_policy.cpp" \
  -o "$OUT/test_strategic_policy"
"$OUT/test_strategic_policy"

for test in test_referee_simulator test_nav_simulator test_decision_actuator_sim; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" -I"$SIM/include" \
    "${CORE_SOURCES[@]}" "${SIM_SOURCES[@]}" "$SIM/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done
