#!/usr/bin/env bash
# 在无 ROS 的宿主机上编译并运行 core 与 sim 的纯逻辑单元测试。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$ROOT/src/sentry_decision_core"
SIM="$ROOT/src/sentry_decision_sim"
OUT="$(mktemp -d)"

CORE_SOURCES=("$CORE/src/action_dispatcher.cpp" "$CORE/src/action_factory.cpp"
  "$CORE/src/logging.cpp" "$CORE/src/nav_goal_tracker.cpp" "$CORE/src/referee_protocol.cpp"
  "$CORE/src/safety_supervisor.cpp" "$CORE/src/task_factory.cpp" "$CORE/src/task_policy.cpp"
  "$CORE/src/task_runner.cpp" "$CORE/src/task_runtime.cpp" "$CORE/src/world_model.cpp"
  "$CORE/src/tasks/heal_task.cpp" "$CORE/src/tasks/navigate_task.cpp"
  "$CORE/src/tasks/outpost_task.cpp" "$CORE/src/tasks/resource_task.cpp"
  "$CORE/src/tasks/respawn_task.cpp" "$CORE/src/tasks/scan_task.cpp")
SIM_SOURCES=("$SIM/src/referee_simulator.cpp" "$SIM/src/nav_simulator.cpp"
  "$SIM/src/decision_actuator_sim.cpp" "$SIM/src/sim_world.cpp" "$SIM/src/match_stage.cpp")

for test in test_logging test_world_model test_referee_protocol test_config test_action_dispatcher test_safety_supervisor test_nav_goal_tracker test_task_runtime test_navigate_scan test_outpost_respawn test_heal_resource test_task_policy test_task_runner; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" "${CORE_SOURCES[@]}" \
    "$CORE/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done

for test in test_referee_simulator test_nav_simulator test_decision_actuator_sim test_sim_world test_match_stage; do
  g++ -std=c++17 -Wall -Wextra -Werror -I"$CORE/include" -I"$SIM/include" \
    "${CORE_SOURCES[@]}" "${SIM_SOURCES[@]}" "$SIM/test/$test.cpp" -o "$OUT/$test"
  "$OUT/$test"
done
