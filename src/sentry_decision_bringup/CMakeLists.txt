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

add_executable(decision_main src/main.cpp)
target_link_libraries(decision_main
  sentry_decision_core::sentry_decision_core
  sentry_decision_nodes::sentry_decision_nodes
  sentry_decision_sim::sentry_decision_sim
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(decision_main PRIVATE
  DEFAULT_TREE_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/tree/demo_tree.xml")

# 真实 IO 决策主程序：RosIoNode + 行为树 + 仲裁 + 决策状态发布。
add_executable(decision_node src/decision_node.cpp)
target_link_libraries(decision_node
  sentry_decision_core::sentry_decision_core
  sentry_decision_nodes::sentry_decision_nodes
  sentry_decision_io::sentry_decision_io
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(decision_node PRIVATE
  DEFAULT_TREE_PATH="${CMAKE_INSTALL_PREFIX}/share/sentry_decision_bringup/tree/demo_tree.xml")

install(TARGETS decision_main decision_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/ DESTINATION share/${PROJECT_NAME}/tree)
install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)

if(BUILD_TESTING)
  add_test(NAME demo_tree
    COMMAND decision_main --ticks 40 --rate 200 --tree
            ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/demo_tree.xml)

  # --rate 越界必须被拒绝（非零退出）。
  add_test(NAME invalid_rate_zero COMMAND decision_main --ticks 1 --rate 0)
  set_tests_properties(invalid_rate_zero PROPERTIES WILL_FAIL TRUE)
  add_test(NAME invalid_rate_high COMMAND decision_main --ticks 1 --rate 2000)
  set_tests_properties(invalid_rate_high PROPERTIES WILL_FAIL TRUE)

  # decision_node 能启动并跑有限 tick 后退出。
  add_test(NAME decision_node_boot
    COMMAND decision_node --ticks 5 --rate 200 --tree
            ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/demo_tree.xml)

  add_executable(test_replay_determinism test/test_replay_determinism.cpp)
  target_link_libraries(test_replay_determinism
    sentry_decision_core::sentry_decision_core
    sentry_decision_nodes::sentry_decision_nodes
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME replay_determinism
    COMMAND test_replay_determinism ${CMAKE_CURRENT_SOURCE_DIR}/../../tree/demo_tree.xml)
endif()

ament_package()
