cmake_minimum_required(VERSION 3.8)
project(wheel_odom)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

include_directories(include)

add_executable(odom_publisher
    src/standalone_odomPublisher.cpp
    src/odomPublisher.cpp
    src/UdpSocket.cpp
    src/SocketAddress.cpp
)

target_include_directories(odom_publisher PUBLIC
    include
)

target_compile_features(odom_publisher PUBLIC cxx_std_20)

ament_target_dependencies(
    odom_publisher
    rclcpp
    nav_msgs
    geometry_msgs
)

install(
    TARGETS odom_publisher
    DESTINATION lib/${PROJECT_NAME}
)

install(
    DIRECTORY launch
    DESTINATION share/${PROJECT_NAME}
)

ament_package()