cmake_minimum_required(VERSION 3.8)
project(wheel_odom)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs)
find_package(std_srvs REQUIRED)
find_package(tinyxml2 REQUIRED)
find_package(ament_index_cpp REQUIRED)


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

target_link_libraries(odom_publisher
    tinyxml2::tinyxml2
)

target_compile_features(odom_publisher PUBLIC cxx_std_20)

ament_target_dependencies(
    odom_publisher
    rclcpp
    nav_msgs
    geometry_msgs
    rclcpp_components
    sensor_msgs
    tf2
    tf2_geometry_msgs
    tf2_ros
    ament_index_cpp
)

install(
    TARGETS odom_publisher
    DESTINATION lib/${PROJECT_NAME}
)

install(
    TARGETS odom_publisher
    DESTINATION lib/${PROJECT_NAME}
)

install(
    DIRECTORY
        config
        launch
    DESTINATION share/${PROJECT_NAME}
)


ament_package()