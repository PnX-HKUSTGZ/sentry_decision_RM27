cmake_minimum_required(VERSION 3.16)
project(sentry_interfaces)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/GameInfo.msg"
  "msg/SentryInfoOnline.msg"
  "msg/SentryInfoOffline.msg"
  "msg/TeamInfo.msg"
  "msg/AllyRobotStatus.msg"
  "msg/RadarInfo.msg"
  "msg/EnemyRobotStatus.msg"
  "msg/DecisionAck.msg"
  "msg/DecisionAction.msg"
  "msg/DecisionCommand.msg"
  DEPENDENCIES std_msgs geometry_msgs)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
