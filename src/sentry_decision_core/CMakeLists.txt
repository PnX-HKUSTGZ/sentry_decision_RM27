cmake_minimum_required(VERSION 3.16)
project(sentry_decision_core)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

add_library(${PROJECT_NAME}
  src/action_dispatcher.cpp
  src/action_factory.cpp
  src/logging.cpp
  src/nav_goal_tracker.cpp
  src/referee_protocol.cpp
  src/safety_supervisor.cpp
  src/tasks/heal_task.cpp
  src/tasks/navigate_task.cpp
  src/tasks/outpost_task.cpp
  src/tasks/resource_task.cpp
  src/tasks/respawn_task.cpp
  src/tasks/scan_task.cpp
  src/task_factory.cpp
  src/task_policy.cpp
  src/task_runner.cpp
  src/task_runtime.cpp
  src/world_model.cpp)
# core 以静态库被 nodes 等共享库链接，必须编译为位置无关代码。
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  add_executable(test_logging test/test_logging.cpp)
  target_link_libraries(test_logging ${PROJECT_NAME})
  add_test(NAME test_logging COMMAND test_logging)

  add_executable(test_world_model test/test_world_model.cpp)
  target_link_libraries(test_world_model ${PROJECT_NAME})
  add_test(NAME test_world_model COMMAND test_world_model)

  add_executable(test_referee_protocol test/test_referee_protocol.cpp)
  target_link_libraries(test_referee_protocol ${PROJECT_NAME})
  add_test(NAME test_referee_protocol COMMAND test_referee_protocol)

  add_executable(test_config test/test_config.cpp)
  target_link_libraries(test_config ${PROJECT_NAME})
  add_test(NAME test_config COMMAND test_config)

  add_executable(test_action_dispatcher test/test_action_dispatcher.cpp)
  target_link_libraries(test_action_dispatcher ${PROJECT_NAME})
  add_test(NAME test_action_dispatcher COMMAND test_action_dispatcher)

  add_executable(test_safety_supervisor test/test_safety_supervisor.cpp)
  target_link_libraries(test_safety_supervisor ${PROJECT_NAME})
  add_test(NAME test_safety_supervisor COMMAND test_safety_supervisor)

  add_executable(test_nav_goal_tracker test/test_nav_goal_tracker.cpp)
  target_link_libraries(test_nav_goal_tracker ${PROJECT_NAME})
  add_test(NAME test_nav_goal_tracker COMMAND test_nav_goal_tracker)

  add_executable(test_task_runtime test/test_task_runtime.cpp)
  target_link_libraries(test_task_runtime ${PROJECT_NAME})
  add_test(NAME test_task_runtime COMMAND test_task_runtime)

  add_executable(test_task_policy test/test_task_policy.cpp)
  target_link_libraries(test_task_policy ${PROJECT_NAME})
  add_test(NAME test_task_policy COMMAND test_task_policy)

  add_executable(test_task_runner test/test_task_runner.cpp)
  target_link_libraries(test_task_runner ${PROJECT_NAME})
  add_test(NAME test_task_runner COMMAND test_task_runner)

  add_executable(test_navigate_scan test/test_navigate_scan.cpp)
  target_link_libraries(test_navigate_scan ${PROJECT_NAME})
  add_test(NAME test_navigate_scan COMMAND test_navigate_scan)

  add_executable(test_outpost_respawn test/test_outpost_respawn.cpp)
  target_link_libraries(test_outpost_respawn ${PROJECT_NAME})
  add_test(NAME test_outpost_respawn COMMAND test_outpost_respawn)

  add_executable(test_heal_resource test/test_heal_resource.cpp)
  target_link_libraries(test_heal_resource ${PROJECT_NAME})
  add_test(NAME test_heal_resource COMMAND test_heal_resource)
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_package()
