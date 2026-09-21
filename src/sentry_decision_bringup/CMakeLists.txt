cmake_minimum_required(VERSION 3.16)
project(sentry_decision_bringup)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(sentry_decision_core REQUIRED)
find_package(sentry_decision_nodes REQUIRED)
find_package(sentry_decision_io REQUIRED)
find_package(sentry_decision_sim REQUIRED)
find_package(yaml-cpp REQUIRED)

# 模块 .so 安装在 sentry_decision_nodes 包前缀下；从其 CMake 配置目录回推 lib 目录。
get_filename_component(SENTRY_NODES_PREFIX "${sentry_decision_nodes_DIR}/../../.." ABSOLUTE)
set(SENTRY_NODES_LIB_DIR "${SENTRY_NODES_PREFIX}/lib")

# 配置 / 清单加载与启动校验（组合根内部静态库，不对外导出）。
add_library(bringup_loader STATIC
  src/config_loader.cpp
  src/tree_loader.cpp)
target_include_directories(bringup_loader PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(bringup_loader PUBLIC
  sentry_decision_core::sentry_decision_core
  sentry_decision_nodes::sentry_decision_common
  sentry_decision_nodes::sentry_decision_nav
  sentry_decision_nodes::sentry_decision_resource
  behaviortree_cpp::behaviortree_cpp
  yaml-cpp::yaml-cpp)

add_executable(decision_main src/main.cpp)
target_link_libraries(decision_main
  bringup_loader
  sentry_decision_core::sentry_decision_core
  sentry_decision_nodes::sentry_decision_common
  sentry_decision_nodes::sentry_decision_nav
  sentry_decision_nodes::sentry_decision_strategic
  sentry_decision_sim::sentry_decision_sim
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(decision_main PRIVATE
  DEFAULT_TREE_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/tree/root.xml"
  DEFAULT_CONFIG_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/config/profiles.yaml"
  DEFAULT_MODULE_LIB_DIR="${SENTRY_NODES_LIB_DIR}")

# 真实 IO 决策主程序：RosIoNode + 行为树 + 仲裁 + 决策状态发布。
add_executable(decision_node src/decision_node.cpp)
target_link_libraries(decision_node
  bringup_loader
  sentry_decision_core::sentry_decision_core
  sentry_decision_nodes::sentry_decision_common
  sentry_decision_nodes::sentry_decision_nav
  sentry_decision_nodes::sentry_decision_strategic
  sentry_decision_io::sentry_decision_io
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(decision_node PRIVATE
  DEFAULT_TREE_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/tree/root.xml"
  DEFAULT_CONFIG_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/config/profiles.yaml"
  DEFAULT_MODULE_LIB_DIR="${SENTRY_NODES_LIB_DIR}")

install(TARGETS decision_main decision_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/ DESTINATION share/${PROJECT_NAME}/tree)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../config/ DESTINATION share/${PROJECT_NAME}/config)
install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)

if(BUILD_TESTING)
  add_test(NAME demo_tree
    COMMAND decision_main --ticks 40 --rate 200
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles.yaml)

  add_test(NAME invalid_rate_zero COMMAND decision_main --ticks 1 --rate 0)
  set_tests_properties(invalid_rate_zero PROPERTIES WILL_FAIL TRUE)
  add_test(NAME invalid_rate_high COMMAND decision_main --ticks 1 --rate 2000)
  set_tests_properties(invalid_rate_high PROPERTIES WILL_FAIL TRUE)

  add_test(NAME config_missing
    COMMAND decision_main --ticks 1
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/does_not_exist.yaml)
  set_tests_properties(config_missing PROPERTIES WILL_FAIL TRUE)

  add_test(NAME tree_bad_config_key
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_key.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles.yaml)
  set_tests_properties(tree_bad_config_key PROPERTIES WILL_FAIL TRUE)

  # 模块消费字段无提供者必须启动失败。
  add_test(NAME module_manifest_missing_provider
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_module/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles.yaml)
  set_tests_properties(module_manifest_missing_provider PROPERTIES WILL_FAIL TRUE)

  # 非法数值（"50oops"）必须启动失败。
  add_test(NAME config_bad_number
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_config_number/profiles.yaml)
  set_tests_properties(config_bad_number PROPERTIES WILL_FAIL TRUE)

  # 点位缺 x/y 必须启动失败。
  add_test(NAME config_bad_point
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_config_point/profiles.yaml)
  set_tests_properties(config_bad_point PROPERTIES WILL_FAIL TRUE)

  # 已知数值键为负必须启动失败。
  add_test(NAME config_bad_negative
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_config_negative/profiles.yaml)
  set_tests_properties(config_bad_negative PROPERTIES WILL_FAIL TRUE)

  # 整数键给出小数必须启动失败。
  add_test(NAME config_bad_fractional
    COMMAND decision_main --ticks 1
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/bad_config_fractional/profiles.yaml)
  set_tests_properties(config_bad_fractional PROPERTIES WILL_FAIL TRUE)

  add_test(NAME decision_node_boot
    COMMAND decision_node --ticks 5 --rate 200
            --tree ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml
            --config ${CMAKE_CURRENT_SOURCE_DIR}/../../config/profiles.yaml)

  add_executable(test_mission_nav test/test_mission_nav.cpp)
  target_link_libraries(test_mission_nav
    sentry_decision_core::sentry_decision_core
    sentry_decision_nodes::sentry_decision_common
    sentry_decision_nodes::sentry_decision_nav
    sentry_decision_nodes::sentry_decision_resource
    sentry_decision_nodes::sentry_decision_strategic
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME mission_nav
    COMMAND test_mission_nav ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml)

  add_executable(test_preemption test/test_preemption.cpp)
  target_link_libraries(test_preemption
    sentry_decision_core::sentry_decision_core
    sentry_decision_nodes::sentry_decision_common
    sentry_decision_nodes::sentry_decision_nav
    sentry_decision_nodes::sentry_decision_resource
    sentry_decision_nodes::sentry_decision_strategic
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME preemption
    COMMAND test_preemption ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml)

  add_executable(test_replay_determinism test/test_replay_determinism.cpp)
  target_link_libraries(test_replay_determinism
    sentry_decision_core::sentry_decision_core
    sentry_decision_nodes::sentry_decision_common
    sentry_decision_nodes::sentry_decision_nav
    sentry_decision_nodes::sentry_decision_resource
    sentry_decision_nodes::sentry_decision_strategic
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME replay_determinism
    COMMAND test_replay_determinism ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/root.xml)
endif()

ament_package()
