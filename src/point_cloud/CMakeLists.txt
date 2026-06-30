cmake_minimum_required(VERSION 3.8)
project(point_cloud)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_sensor_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(std_srvs REQUIRED)
find_package(PCL REQUIRED COMPONENTS common io filters)

include_directories(include ${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

add_executable(pointcloud_fusion_node
  src/point_cloud_fusion_node.cpp
  src/point_cloud_fusion_main.cpp
)

ament_target_dependencies(pointcloud_fusion_node rclcpp)
ament_target_dependencies(pointcloud_fusion_node sensor_msgs)
ament_target_dependencies(pointcloud_fusion_node geometry_msgs)
ament_target_dependencies(pointcloud_fusion_node tf2)
ament_target_dependencies(pointcloud_fusion_node tf2_ros)
ament_target_dependencies(pointcloud_fusion_node tf2_sensor_msgs)
ament_target_dependencies(pointcloud_fusion_node pcl_conversions)
ament_target_dependencies(pointcloud_fusion_node std_srvs)


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

 
install(DIRECTORY
  launch
  # config
  DESTINATION share/${PROJECT_NAME}
)
install(TARGETS
  pointcloud_fusion_node
  DESTINATION lib/${PROJECT_NAME}
)

# Your existing launch folder install block follows below:
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)
ament_package()
