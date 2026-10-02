cmake_minimum_required(VERSION 3.16)
project(sentry_decision_viz)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(sentry_decision_msgs REQUIRED)

add_library(${PROJECT_NAME}
  src/tree_state.cpp
  src/tree_state_publisher.cpp
  src/groot2_bridge.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(${PROJECT_NAME}
  behaviortree_cpp::behaviortree_cpp
  sentry_decision_msgs::sentry_decision_msgs)
ament_target_dependencies(${PROJECT_NAME} rclcpp)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY web/ DESTINATION share/${PROJECT_NAME}/web)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(behaviortree_cpp sentry_decision_msgs rclcpp)

if(BUILD_TESTING)
  add_executable(test_tree_state test/test_tree_state.cpp)
  target_link_libraries(test_tree_state ${PROJECT_NAME})
  add_test(NAME test_tree_state COMMAND test_tree_state)

  # 网页面板端到端冒烟；未安装 rosbridge_server 时脚本自行跳过。
  add_test(NAME viz_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/viz_smoke_test.sh)
  set_tests_properties(viz_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 120)
endif()

ament_package()
