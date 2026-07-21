cmake_minimum_required(VERSION 3.8)

project(websocket_bridge)


# ------------------------------------------------------------
# Compiler settings
# ------------------------------------------------------------

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()


# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------

find_package(ament_cmake REQUIRED)

find_package(rclcpp REQUIRED)

find_package(nav_msgs REQUIRED)

find_package(sensor_msgs REQUIRED)

find_package(geometry_msgs REQUIRED)

find_package(ament_index_cpp REQUIRED)

find_package(TinyXML2 REQUIRED)


# ------------------------------------------------------------
# Executable
# ------------------------------------------------------------

add_executable(websocket_bridge

    src/bridge_standalone.cpp

    src/websocket_bridge.cpp
)


# ------------------------------------------------------------
# Include directories
# ------------------------------------------------------------

target_include_directories(websocket_bridge
    PUBLIC
        include
)


# ------------------------------------------------------------
# ROS dependencies
# ------------------------------------------------------------

ament_target_dependencies(websocket_bridge

    rclcpp

    nav_msgs

    sensor_msgs

    geometry_msgs

    ament_index_cpp
)


# ------------------------------------------------------------
# TinyXML2
# ------------------------------------------------------------

target_link_libraries(websocket_bridge
    tinyxml2::tinyxml2
)


# ------------------------------------------------------------
# Install executable
# ------------------------------------------------------------

install(
    TARGETS websocket_bridge

    DESTINATION
        lib/${PROJECT_NAME}
)


# ------------------------------------------------------------
# Install headers
# ------------------------------------------------------------

install(
    DIRECTORY include/

    DESTINATION
        include
)


# ------------------------------------------------------------
# Install config and launch files
# ------------------------------------------------------------

install(
    DIRECTORY
        config
        launch

    DESTINATION
        share/${PROJECT_NAME}
)


# ------------------------------------------------------------
# Package
# ------------------------------------------------------------

ament_package()