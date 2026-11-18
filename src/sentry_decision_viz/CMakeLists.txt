cmake_minimum_required(VERSION 3.16)
project(sentry_decision_viz)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)

# 本包只有静态资源，没有编译单元；显式引用该变量，避免 colcon 传入时
# CMake 报「变量未使用」警告。
set(_viz_export_compile_commands "${CMAKE_EXPORT_COMPILE_COMMANDS}")

install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY web/ DESTINATION share/${PROJECT_NAME}/web)

if(BUILD_TESTING)
  # 网页面板端到端冒烟；未安装 rosbridge_server 时脚本自行跳过。
  add_test(NAME viz_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/viz_smoke_test.sh)
  set_tests_properties(viz_smoke PROPERTIES
    ENVIRONMENT "SCENARIO_OVERLAY_DIR=${CMAKE_INSTALL_PREFIX}/.."
    TIMEOUT 120)
endif()

ament_package()
