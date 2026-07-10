cmake_minimum_required(VERSION 3.8)
project(fast_point_cloud)

find_package(ament_cmake REQUIRED)

install(
  DIRECTORY launch config description
  DESTINATION share/${PROJECT_NAME}
)

install(
  PROGRAMS
    scripts/surround_view_node.py
    scripts/camera_watchdog.py
    scripts/wait_for_stable_pose.py
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
