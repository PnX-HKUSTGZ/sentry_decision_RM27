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
  src/match_stage.cpp)
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
    COMMAND test_scenario ${CMAKE_CURRENT_SOURCE_DIR}/scenario/full_match.yaml)

  # 端到端：decision_node + referee_sim_node 跑完整场景时间轴并断言。
  add_test(NAME scenario_full_match
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/full_match.yaml)
  set_tests_properties(scenario_full_match PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

  # 端到端：场景 add_intent / disable 注入干预（干预即测试用例）。
  add_test(NAME scenario_intervention
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/scenario_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/intervention.yaml)
  set_tests_properties(scenario_intervention PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

  # 端到端：录制一局场景 bag，再用 replay_main 离线重放（含人工干预）。
  add_test(NAME replay_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/replay_smoke_test.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/scenario/intervention.yaml)
  set_tests_properties(replay_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 150)

  # 端到端：干预 service / action 与 /decision/intervention。
  add_test(NAME intervention_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/intervention_smoke_test.sh)
  set_tests_properties(intervention_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 90)

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

  # 这些端到端用例各自拉起 ROS 图，串行执行，避免并发抢占导致的超时抖动。
  set_tests_properties(
    scenario_full_match scenario_intervention replay_smoke intervention_smoke match_smoke
    sim_effects_smoke referee_guard_smoke
    PROPERTIES RUN_SERIAL TRUE)
endif()

ament_package()
