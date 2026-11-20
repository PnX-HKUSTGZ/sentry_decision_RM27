cmake_minimum_required(VERSION 3.16)
project(sentry_decision_sim)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(sentry_decision_core REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sentry_interfaces REQUIRED)
find_package(sentry_decision_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

# ROS 无关的仿真组件：dummy 裁判、伪导航、动作执行端、场景解析与消息级世界。
add_library(${PROJECT_NAME}
  src/referee_simulator.cpp
  src/nav_simulator.cpp
  src/decision_actuator_sim.cpp
  src/scenario.cpp
  src/sim_world.cpp
  src/match_stage.cpp
  src/sim_config.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
ament_target_dependencies(${PROJECT_NAME} sentry_decision_core)
# 与 ament_target_dependencies 的 plain 签名保持一致，不使用 PRIVATE 关键字。
target_link_libraries(${PROJECT_NAME} yaml-cpp::yaml-cpp)

# ROS 侧仿真节点：把 ROS 无关的仿真组件接到 /sentry/* 与导航 action 上。
add_executable(referee_sim_node src/referee_sim_node.cpp)
target_link_libraries(referee_sim_node ${PROJECT_NAME})
ament_target_dependencies(referee_sim_node
  rclcpp rclcpp_action nav2_msgs nav_msgs geometry_msgs std_msgs
  sentry_interfaces sentry_decision_msgs sentry_decision_core)
# 仿真参数 yaml 的编译期默认路径（开发/仿真用；可用 --sim-config 覆盖）。
target_compile_definitions(referee_sim_node PRIVATE
  DEFAULT_SIM_CONFIG_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../config/sim.yaml")

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS referee_sim_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY scenario/ DESTINATION share/${PROJECT_NAME}/scenario)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(sentry_decision_core)

if(BUILD_TESTING)
  add_executable(test_decision_actuator_sim test/test_decision_actuator_sim.cpp)
  target_link_libraries(test_decision_actuator_sim ${PROJECT_NAME})
  add_test(NAME test_decision_actuator_sim COMMAND test_decision_actuator_sim)

  add_executable(test_referee_simulator test/test_referee_simulator.cpp)
  target_link_libraries(test_referee_simulator ${PROJECT_NAME})
  add_test(NAME test_referee_simulator COMMAND test_referee_simulator)

  add_executable(test_nav_simulator test/test_nav_simulator.cpp)
  target_link_libraries(test_nav_simulator ${PROJECT_NAME})
  add_test(NAME test_nav_simulator COMMAND test_nav_simulator)

  add_executable(test_match_stage test/test_match_stage.cpp)
  target_link_libraries(test_match_stage ${PROJECT_NAME})
  add_test(NAME test_match_stage COMMAND test_match_stage)

  add_executable(test_sim_world test/test_sim_world.cpp)
  target_link_libraries(test_sim_world ${PROJECT_NAME})
  add_test(NAME test_sim_world COMMAND test_sim_world)

  add_executable(test_scenario test/test_scenario.cpp)
  target_link_libraries(test_scenario ${PROJECT_NAME} yaml-cpp::yaml-cpp)
  add_test(NAME test_scenario
    COMMAND test_scenario ${CMAKE_CURRENT_SOURCE_DIR}/scenario/demo.yaml)


  # 端到端：裁判仿真的比赛阶段服务（前进可快进、回退被拒、重置回未开始）。
  add_test(NAME match_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/match_smoke_test.sh)
  set_tests_properties(match_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

  # 端到端：裁判仿真的世界结算（兑换发弹真正加弹 / 扣金币，补给区自动回血）。
  add_test(NAME sim_effects_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/sim_effects_smoke_test.sh)
  set_tests_properties(sim_effects_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

  # 端到端：裁判系统按前置条件拒绝非法动作（未占领增益点 / 未脱战）并打印日志。
  add_test(NAME referee_guard_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/referee_guard_smoke_test.sh)
  set_tests_properties(referee_guard_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

  # 端到端：无头跑完整策略流程（进攻前哨 / Scan / Heal / Resource / Respawn）。
  add_test(NAME strategy_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/strategy_smoke.yaml)
  set_tests_properties(strategy_smoke PROPERTIES TIMEOUT 120)

  # 端到端：utility policy 在敌方高地射击位选择 Outpost，打掉后回防。
  add_test(NAME utility_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/utility_smoke.yaml)
  set_tests_properties(utility_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_TASK_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles_rmuc27_utility.yaml"
    TIMEOUT 30)

  # 端到端：己方前哨可用时，在己方高地低血应就近去前哨站增益点。
  add_test(NAME heal_outpost_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/heal_outpost_smoke.yaml)
  set_tests_properties(heal_outpost_smoke PROPERTIES TIMEOUT 25)

  # 端到端：同一回血选点在 utility 策略下也应成立。
  add_test(NAME heal_outpost_utility_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/heal_outpost_smoke.yaml)
  set_tests_properties(heal_outpost_utility_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_TASK_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles_rmuc27_utility.yaml"
    TIMEOUT 25)

  # 端到端：utility 低血/低弹时从被毁前哨 buff 回基地恢复。
  add_test(NAME utility_recover_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/utility_recover_smoke.yaml)
  set_tests_properties(utility_recover_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_TASK_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles_rmuc27_utility.yaml"
    TIMEOUT 35)

  # 端到端：utility 在敌方 opposite 阵亡，复活后自动 Heal 回基地。
  add_test(NAME utility_respawn_recover_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/utility_respawn_recover_smoke.yaml)
  set_tests_properties(utility_respawn_recover_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_TASK_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles_rmuc27_utility.yaml"
    TIMEOUT 40)

  # 端到端：utility 在己方前哨掉到临时控制时从敌方半场回防。
  add_test(NAME utility_defense_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/utility_defense_smoke.yaml)
  set_tests_properties(utility_defense_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_TASK_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles_rmuc27_utility.yaml"
    TIMEOUT 45)

  # 端到端：前哨控制位从 event_data 解码到 Task 层，控制后不再进攻。
  add_test(NAME control_gate_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/control_gate.yaml)
  set_tests_properties(control_gate_smoke PROPERTIES TIMEOUT 30)

  # 端到端：低弹在基地走 Resource 而不是 Heal 空转；重置后可重新进入任务。
  add_test(NAME low_ammo_reset_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/low_ammo_reset.yaml)
  set_tests_properties(low_ammo_reset_smoke PROPERTIES TIMEOUT 30)

  # 端到端：导航成功并进入 Outpost 后重置，重新开始仍能 Navigate。
  add_test(NAME nav_reset_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/nav_reset_smoke_test.sh)
  set_tests_properties(nav_reset_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 40)

  # 这些端到端用例各自拉起 ROS 图，串行执行，避免并发抢占导致的超时抖动。
  set_tests_properties(
    match_smoke sim_effects_smoke referee_guard_smoke strategy_smoke utility_smoke
    heal_outpost_smoke heal_outpost_utility_smoke
    utility_defense_smoke utility_recover_smoke utility_respawn_recover_smoke
    control_gate_smoke low_ammo_reset_smoke nav_reset_smoke
    PROPERTIES RUN_SERIAL TRUE)
endif()

ament_package()
