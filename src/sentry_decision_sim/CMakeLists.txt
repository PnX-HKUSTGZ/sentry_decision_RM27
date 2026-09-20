cmake_minimum_required(VERSION 3.16)
project(sentry_decision_sim)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(sentry_decision_core REQUIRED)

add_library(${PROJECT_NAME}
  src/referee_simulator.cpp
  src/nav_simulator.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
ament_target_dependencies(${PROJECT_NAME} sentry_decision_core)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(sentry_decision_core)

if(BUILD_TESTING)
  add_executable(test_referee_simulator test/test_referee_simulator.cpp)
  target_link_libraries(test_referee_simulator ${PROJECT_NAME})
  add_test(NAME test_referee_simulator COMMAND test_referee_simulator)

  add_executable(test_nav_simulator test/test_nav_simulator.cpp)
  target_link_libraries(test_nav_simulator ${PROJECT_NAME})
  add_test(NAME test_nav_simulator COMMAND test_nav_simulator)
endif()

ament_package()
