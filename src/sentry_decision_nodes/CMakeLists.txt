cmake_minimum_required(VERSION 3.16)
project(sentry_decision_nodes)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(sentry_decision_core REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/nodes.cpp
  src/rule_based_strategic_policy.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(${PROJECT_NAME} PUBLIC
  sentry_decision_core::sentry_decision_core
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE BT_PLUGIN_EXPORT)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  add_executable(test_nodes test/test_nodes.cpp)
  target_link_libraries(test_nodes ${PROJECT_NAME})
  add_test(NAME test_nodes COMMAND test_nodes)

  add_executable(test_strategic_policy test/test_strategic_policy.cpp)
  target_link_libraries(test_strategic_policy ${PROJECT_NAME})
  add_test(NAME test_strategic_policy COMMAND test_strategic_policy)
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(sentry_decision_core behaviortree_cpp)
ament_package()
