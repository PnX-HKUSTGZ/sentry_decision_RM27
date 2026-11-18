cmake_minimum_required(VERSION 3.16)
project(sentry_decision_bringup)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(sentry_decision_core REQUIRED)
find_package(sentry_decision_io REQUIRED)
find_package(yaml-cpp REQUIRED)

# 配置加载（组合根内部静态库，不对外导出）。
add_library(bringup_loader STATIC src/config_loader.cpp)
target_include_directories(bringup_loader PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(bringup_loader PUBLIC
  sentry_decision_core::sentry_decision_core
  yaml-cpp::yaml-cpp)

# 2027 Task 层主程序：RosIoNode + TaskRunner + SafetySupervisor + 决策状态发布。
add_executable(task_node src/task_node.cpp)
target_link_libraries(task_node
  bringup_loader
  sentry_decision_core::sentry_decision_core
  sentry_decision_io::sentry_decision_io)
target_compile_definitions(task_node PRIVATE
  DEFAULT_CONFIG_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/config/profiles.yaml")

install(TARGETS task_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../config/ DESTINATION share/${PROJECT_NAME}/config)
install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)

if(BUILD_TESTING)
  add_test(NAME task_node_smoke
    COMMAND task_node --ticks 1
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles.yaml)
  set_tests_properties(task_node_smoke PROPERTIES TIMEOUT 20)
endif()

ament_package()
