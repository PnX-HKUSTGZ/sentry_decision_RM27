cmake_minimum_required(VERSION 3.16)
project(sentry_decision_nodes)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(sentry_decision_core REQUIRED)

# 功能域模块：每个编成独立 .so，各自 BT_REGISTER_NODES 自注册。
# common：条件 + 通用 Intent 叶子节点。
add_library(sentry_decision_common SHARED src/common_nodes.cpp)
target_include_directories(sentry_decision_common PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(sentry_decision_common PUBLIC
  sentry_decision_core::sentry_decision_core
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(sentry_decision_common PRIVATE BT_PLUGIN_EXPORT)

# nav：导航技能节点。
add_library(sentry_decision_nav SHARED src/nav_nodes.cpp)
target_include_directories(sentry_decision_nav PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(sentry_decision_nav PUBLIC
  sentry_decision_core::sentry_decision_core
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(sentry_decision_nav PRIVATE BT_PLUGIN_EXPORT)

# resource：资源 / 复活技能节点。
add_library(sentry_decision_resource SHARED src/resource_nodes.cpp)
target_include_directories(sentry_decision_resource PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(sentry_decision_resource PUBLIC
  sentry_decision_core::sentry_decision_core
  behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(sentry_decision_resource PRIVATE BT_PLUGIN_EXPORT)

# strategic：战略策略实现（纯 C++，非 BT 插件）。
add_library(sentry_decision_strategic SHARED src/rule_based_strategic_policy.cpp)
target_include_directories(sentry_decision_strategic PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(sentry_decision_strategic PUBLIC
  sentry_decision_core::sentry_decision_core)

install(TARGETS
  sentry_decision_common
  sentry_decision_nav
  sentry_decision_resource
  sentry_decision_strategic
  EXPORT export_${PROJECT_NAME}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  add_executable(test_nodes test/test_nodes.cpp)
  target_link_libraries(test_nodes
    sentry_decision_common
    sentry_decision_nav
    sentry_decision_resource
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME test_nodes COMMAND test_nodes)

  add_executable(test_resource test/test_resource.cpp)
  target_link_libraries(test_resource
    sentry_decision_common
    sentry_decision_resource
    behaviortree_cpp::behaviortree_cpp)
  add_test(NAME test_resource COMMAND test_resource)

  add_executable(test_strategic_policy test/test_strategic_policy.cpp)
  target_link_libraries(test_strategic_policy sentry_decision_strategic)
  add_test(NAME test_strategic_policy COMMAND test_strategic_policy)
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(sentry_decision_core behaviortree_cpp)
ament_package()
