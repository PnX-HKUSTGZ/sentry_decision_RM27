cmake_minimum_required(VERSION 3.16)
project(sentry_decision_msgs)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/DecisionOutput.msg"
  "msg/DecisionState.msg"
  "msg/InterventionEvent.msg"
  "msg/TreeNodeStatus.msg"
  "msg/TreeStatus.msg"
  "msg/WorldState.msg"
  "action/ManualOverride.action"
  "srv/DebugCommand.srv"
  DEPENDENCIES std_msgs geometry_msgs action_msgs unique_identifier_msgs)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
