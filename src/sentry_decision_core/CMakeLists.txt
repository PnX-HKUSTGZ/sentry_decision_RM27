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
  src/arbiter.cpp
  src/intervention.cpp
  src/logging.cpp
  src/referee_protocol.cpp
  src/replay.cpp
  src/safety_supervisor.cpp
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
  add_executable(test_arbiter test/test_arbiter.cpp)
  target_link_libraries(test_arbiter ${PROJECT_NAME})
  add_test(NAME test_arbiter COMMAND test_arbiter)

  add_executable(test_logging test/test_logging.cpp)
  target_link_libraries(test_logging ${PROJECT_NAME})
  add_test(NAME test_logging COMMAND test_logging)

  add_executable(test_world_model test/test_world_model.cpp)
  target_link_libraries(test_world_model ${PROJECT_NAME})
  add_test(NAME test_world_model COMMAND test_world_model)

  add_executable(test_referee_protocol test/test_referee_protocol.cpp)
  target_link_libraries(test_referee_protocol ${PROJECT_NAME})
  add_test(NAME test_referee_protocol COMMAND test_referee_protocol)

  add_executable(test_replay test/test_replay.cpp)
  target_link_libraries(test_replay ${PROJECT_NAME})
  add_test(NAME test_replay COMMAND test_replay)

  add_executable(test_config test/test_config.cpp)
  target_link_libraries(test_config ${PROJECT_NAME})
  add_test(NAME test_config COMMAND test_config)

  add_executable(test_action_dispatcher test/test_action_dispatcher.cpp)
  target_link_libraries(test_action_dispatcher ${PROJECT_NAME})
  add_test(NAME test_action_dispatcher COMMAND test_action_dispatcher)

  add_executable(test_safety_supervisor test/test_safety_supervisor.cpp)
  target_link_libraries(test_safety_supervisor ${PROJECT_NAME})
  add_test(NAME test_safety_supervisor COMMAND test_safety_supervisor)

  add_executable(test_intervention test/test_intervention.cpp)
  target_link_libraries(test_intervention ${PROJECT_NAME})
  add_test(NAME test_intervention COMMAND test_intervention)
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_package()
