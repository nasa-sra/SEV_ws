# WebSocket Bridge

The `websocket_bridge` package is a ROS 2 node that collects robot state and sensor data from multiple ROS 2 topics, packages the required data into a compact JSON message, and sends the JSON data to an external application over a WebSocket connection.

The bridge subscribes to:

* `/odom`
* `/vectornav/pose`
* `/vectornav/imu`
* `/vectornav/gnss`

The `/vectornav/gnss` topic provides geographic position data, including:

* Latitude
* Longitude

The latest available data is packaged into a compact JSON message and transmitted to a WebSocket server at a configurable rate.

---

## Overview

The overall data flow is:

```text
                         ROS 2
                           │
       ┌───────────────────┼───────────────────────┐
       │                   │                       │
       ▼                   ▼                       ▼
    /odom          /vectornav/pose        /vectornav/imu
       │                   │                       │
       │                   │                       │
       └───────────────────┼───────────────────────┘
                           │
                           │
                           ▼
                    /vectornav/gnss
                           │
                           │
                           ▼
                  websocket_bridge
                           │
                           │ Build Compact JSON
                           │
                           │ Includes:
                           │ • Robot state
                           │ • IMU data
                           │ • Latitude
                           │ • Longitude
                           ▼
                      JSON Message
                           │
                           │ WebSocket
                           ▼
                  WebSocket Server
                           │
                           ▼
                  External Application
```

The WebSocket bridge acts as a **WebSocket client**. A WebSocket server must be running and listening on the configured address and port.

---

# Package Structure

The package is organized as follows:

```text
websocket_bridge/
├── CMakeLists.txt
├── package.xml
├── README.md
│
├── config/
│   └── websocket_bridge.xml
│
├── include/
│   └── websocket_bridge.hpp
│
├── launch/
│   └── websocket_bridge.launch.py
│
└── src/
    ├── bridge_standalone.cpp
    └── websocket_bridge.cpp
```

## File Descriptions

### `src/bridge_standalone.cpp`

Contains the `main()` function for the ROS 2 node.

This file:

1. Initializes ROS 2.
2. Creates the `WebsocketBridge` node.
3. Spins the node.
4. Shuts down ROS 2 when the node exits.

---

### `src/websocket_bridge.cpp`

Contains the primary implementation of the WebSocket bridge.

Responsibilities include:

* Creating ROS 2 subscribers.
* Receiving `/odom` messages.
* Receiving `/vectornav/pose` messages.
* Receiving `/vectornav/imu` messages.
* Receiving `/vectornav/gnss` messages.
* Storing the latest received data.
* Loading the XML configuration.
* Building the compact JSON payload.
* Connecting to the WebSocket server.
* Sending JSON data over the WebSocket connection.

---

### `include/websocket_bridge.hpp`

Contains the `WebsocketBridge` class declaration.

This includes declarations for:

* ROS 2 subscribers.
* ROS 2 message storage.
* ROS 2 callbacks.
* WebSocket client functionality.
* JSON generation.
* Configuration variables.
* Thread synchronization.

---

### `config/websocket_bridge.xml`

Contains the WebSocket bridge configuration.

The configuration specifies information such as:

* WebSocket server address.
* WebSocket server port.
* JSON publishing rate.

The configuration is installed with the ROS 2 package and loaded by the C++ node at runtime.

---

### `launch/websocket_bridge.launch.py`

Launches the `websocket_bridge` ROS 2 node.

The node loads `websocket_bridge.xml` directly from the installed package configuration directory.

---

# ROS 2 Topics

The bridge subscribes to the following ROS 2 topics:

| Topic             | Purpose                                                                  |
| ----------------- | ------------------------------------------------------------------------ |
| `/odom`           | Provides robot odometry data                                             |
| `/vectornav/pose` | Provides VectorNav pose and position data                                |
| `/vectornav/imu`  | Provides VectorNav IMU data                                              |
| `/vectornav/gnss` | Provides GNSS geographic position data, including latitude and longitude |

---

## `/odom`

Message type:

```text
nav_msgs/msg/Odometry
```

This topic provides wheel or robot odometry information.

The bridge uses the latest odometry data when constructing the JSON message.

Depending on the current implementation, this may include information such as:

* Position.
* Orientation.
* Linear velocity.
* Angular velocity.

---

## `/vectornav/pose`

Message type:

```text
geometry_msgs/msg/PoseWithCovarianceStamped
```

This topic is expected to be published by the VectorNav ROS 2 node.

The bridge uses the latest VectorNav pose information when constructing the JSON message.

The message structure is:

```text
PoseWithCovarianceStamped
├── header
└── pose
    ├── pose
    │   ├── position
    │   └── orientation
    └── covariance
```

Because the message type is `PoseWithCovarianceStamped`, pose data is accessed through the additional `pose` layer.

For example:

```cpp
pose.pose.pose.position.x
```

rather than:

```cpp
pose.pose.position.x
```

---

## `/vectornav/imu`

Message type:

```text
sensor_msgs/msg/Imu
```

This topic is expected to be published by the VectorNav ROS 2 node.

The bridge uses the latest IMU information when constructing the JSON message.

Depending on the current JSON implementation, this may include information such as:

* Orientation.
* Angular velocity.
* Linear acceleration.

---

## `/vectornav/gnss`

This topic provides GNSS information from the VectorNav system.

The bridge uses the latest GNSS data when constructing the outgoing JSON message.

The GNSS data provides geographic position information, including:

* Latitude.
* Longitude.

The latitude and longitude values are included in the compact JSON message sent over the WebSocket connection.

The exact message type and fields should match the VectorNav ROS 2 publisher used by the system.

---

# Geographic Position Data

Geographic position information is obtained from:

```text
/vectornav/gnss
```

The GNSS callback receives the latest geographic position data and makes the relevant information available to the JSON builder.

The data flow is:

```text
/vectornav/gnss
       │
       │ GNSS Message
       ▼
gnssCallback()
       │
       ├── Latitude
       │
       └── Longitude
              │
              ▼
         buildJson()
              │
              ▼
      Compact JSON Message
              │
              ▼
       WebSocket Server
```

The latitude and longitude values are sent as part of the WebSocket JSON payload.

---

# JSON Data

The bridge packages the required robot state and sensor information into a **compact JSON object** before sending it over the WebSocket connection.

The JSON payload has been intentionally reduced to only the data needed by the receiving application. This reduces message size and minimizes unnecessary network traffic.

The payload is built from the latest available data from:

* `/odom`
* `/vectornav/pose`
* `/vectornav/imu`
* `/vectornav/gnss`

The GNSS data provides:

* Latitude.
* Longitude.

The exact JSON structure is defined by the implementation of:

```cpp
buildJson()
```

in:

```text
src/websocket_bridge.cpp
```

A simplified example of the type of information transmitted is:

```json
{
    "latitude": 29.7604,
    "longitude": -95.3698,
    "odom": {
        "x": 1.23,
        "y": 4.56
    },
    "imu": {
        "ax": 0.1,
        "ay": 0.2,
        "az": 9.81
    }
}
```

The exact field names and structure depend on the current implementation of `buildJson()`.

The JSON is sent as a WebSocket text message.

The receiving application should parse each incoming WebSocket message as standard JSON.

---

# Data Flow

The bridge stores the latest available data received from its ROS 2 subscriptions.

The general process is:

```text
ROS 2 Topic Publishes
        │
        ├── /odom
        │
        ├── /vectornav/pose
        │
        ├── /vectornav/imu
        │
        └── /vectornav/gnss
                │
                ▼
          ROS 2 Callbacks
                │
                ▼
         Store Latest Data
                │
                ▼
           buildJson()
                │
                ▼
        Compact JSON Message
                │
                ▼
         WebSocket Send
                │
                ▼
       External Application
```

The bridge uses the latest available data when creating the JSON message.

---

# WebSocket Configuration

The bridge uses the following XML configuration file:

```text
config/websocket_bridge.xml
```

The configuration file specifies the WebSocket connection and bridge settings.

An example configuration is:

```xml
<?xml version="1.0"?>
<websocket_config>

    <websocket>
        <host>127.0.0.1</host>
        <port>9002</port>
    </websocket>

    <publish_rate>10.0</publish_rate>

</websocket_config>
```

The exact XML structure must match the configuration parser implemented in `websocket_bridge.cpp`.

---

## WebSocket Host

The host specifies the IP address of the WebSocket server.

For a WebSocket server running on the same computer:

```xml
<host>127.0.0.1</host>
```

For a WebSocket server running on another computer, use the IP address of that computer.

For example:

```xml
<host>192.168.0.100</host>
```

---

## WebSocket Port

The port specifies the port on which the WebSocket server is listening.

For example:

```xml
<port>9002</port>
```

The WebSocket server must listen on the same port.

---

## Publish Rate

The publish rate controls how frequently the bridge sends JSON data.

For example:

```xml
<publish_rate>10.0</publish_rate>
```

means the bridge attempts to send approximately:

```text
10 messages per second
```

or:

```text
10 Hz
```

---

# WebSocket Connection

The bridge acts as a **WebSocket client**.

For example, with the default configuration:

```text
ws://127.0.0.1:9002
```

the bridge attempts to connect to:

```text
IP Address: 127.0.0.1
Port:       9002
```

The architecture is:

```text
websocket_bridge
      │
      │ WebSocket Client
      │
      ▼
ws://127.0.0.1:9002
      │
      │ WebSocket Server
      ▼
External Application
```

The bridge does **not** create the WebSocket server.

An external application must provide the WebSocket server.

If no server is listening on the configured host and port, the bridge may report:

```text
Connection refused
```

This means that the bridge attempted to establish a connection, but no WebSocket server accepted the connection.

---

# Building the Package

From the ROS 2 workspace:

```bash
cd ~/SEV_ws
```

Build the WebSocket bridge:

```bash
colcon build --packages-select websocket_bridge
```

After a successful build, source the workspace:

```bash
source install/setup.bash
```

If build or CMake configuration issues occur, clean the package build:

```bash
rm -rf build/websocket_bridge
```

Then rebuild:

```bash
colcon build --packages-select websocket_bridge
```

Source the workspace again:

```bash
source install/setup.bash
```

---

# Running the Bridge

The recommended method is to launch the node using the ROS 2 launch file:

```bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

Expected output is similar to:

```text
[INFO] [websocket_bridge]: Starting WebSocket Bridge
[INFO] [websocket_bridge]: Loading configuration from:
.../install/websocket_bridge/share/websocket_bridge/config/websocket_bridge.xml
[INFO] [websocket_bridge]: WebSocket URI: ws://127.0.0.1:9002
[INFO] [websocket_bridge]: Publish rate: 10.00 Hz
[INFO] [websocket_bridge]: Connecting to WebSocket server: ws://127.0.0.1:9002
```

---

# Testing ROS 2 Topics

Check whether `/odom` is available:

```bash
ros2 topic info /odom
```

Check whether `/vectornav/pose` is available:

```bash
ros2 topic info /vectornav/pose
```

Check whether `/vectornav/imu` is available:

```bash
ros2 topic info /vectornav/imu
```

Check whether `/vectornav/gnss` is available:

```bash
ros2 topic info /vectornav/gnss
```

You can inspect the actual messages with:

```bash
ros2 topic echo /odom
```

```bash
ros2 topic echo /vectornav/pose
```

```bash
ros2 topic echo /vectornav/imu
```

```bash
ros2 topic echo /vectornav/gnss
```

To list all active ROS 2 topics:

```bash
ros2 topic list
```

---

# Testing the WebSocket Connection

A simple Python WebSocket server can be used to test the bridge before connecting it to the final external application.

Install the Python WebSocket package:

```bash
python3 -m pip install --user websockets
```

Create a file named:

```text
websocket_test_server.py
```

The server should listen on the same host and port configured in:

```text
config/websocket_bridge.xml
```

For the default configuration:

```text
ws://127.0.0.1:9002
```

The test server can print incoming JSON messages to the terminal.

Once the bridge connects successfully, the server should report that a client connected.

When the bridge begins sending data, the server should display the compact JSON messages containing the data selected by `buildJson()`, including the available latitude and longitude values.

---

# Troubleshooting

## `Connection refused`

Example:

```text
asio async_connect error: system:111 (Connection refused)
```

This means the bridge cannot connect to the configured WebSocket server.

Check that:

1. The WebSocket server is running.
2. The server is listening on the correct IP address.
3. The server is listening on the correct port.
4. The values in `config/websocket_bridge.xml` match the WebSocket server.

For example, if the bridge is configured for:

```text
ws://127.0.0.1:9002
```

the WebSocket server must listen on:

```text
127.0.0.1:9002
```

---

## No JSON messages are being received

First verify that the WebSocket connection is established.

Then verify that the ROS topics exist:

```bash
ros2 topic list
```

Check each topic:

```bash
ros2 topic info /odom
ros2 topic info /vectornav/pose
ros2 topic info /vectornav/imu
ros2 topic info /vectornav/gnss
```

If necessary, inspect the messages directly:

```bash
ros2 topic echo /odom
```

```bash
ros2 topic echo /vectornav/pose
```

```bash
ros2 topic echo /vectornav/imu
```

```bash
ros2 topic echo /vectornav/gnss
```

If the ROS topics are not publishing, the bridge will not have new sensor data to package.

---

## VectorNav Pose Type Mismatch

The VectorNav pose topic is expected to use:

```text
geometry_msgs/msg/PoseWithCovarianceStamped
```

The subscriber therefore uses:

```cpp
geometry_msgs::msg::PoseWithCovarianceStamped
```

The required include is:

```cpp
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
```

Position data is accessed through:

```cpp
pose.pose.pose.position
```

rather than:

```cpp
pose.pose.position
```

because `PoseWithCovarianceStamped` contains an additional `PoseWithCovariance` layer.

---

## GNSS Data Not Appearing

If latitude or longitude is missing from the JSON message, verify that `/vectornav/gnss` is publishing:

```bash
ros2 topic info /vectornav/gnss
```

Then inspect the incoming data:

```bash
ros2 topic echo /vectornav/gnss
```

If the topic is not publishing, verify that the VectorNav node is running and configured to publish GNSS data.

Also verify that the GNSS callback in `websocket_bridge` is receiving the expected message type and that `buildJson()` is using the latest GNSS values.

---

## Configuration File Not Found

The bridge expects the XML configuration to be installed with the package.

After building, verify that the configuration file exists:

```bash
ls ~/SEV_ws/install/websocket_bridge/share/websocket_bridge/config/
```

You should see:

```text
websocket_bridge.xml
```

The expected installed path is:

```text
~/SEV_ws/install/websocket_bridge/share/websocket_bridge/config/websocket_bridge.xml
```

The `config` directory should be installed by `CMakeLists.txt`:

```cmake
install(
    DIRECTORY
        config
        launch
    DESTINATION share/${PROJECT_NAME}
)
```

After changing installation rules, rebuild:

```bash
colcon build --packages-select websocket_bridge
```

---

# Dependencies

The package uses the following ROS 2 dependencies:

* `rclcpp`
* `nav_msgs`
* `sensor_msgs`
* `geometry_msgs`
* `ament_index_cpp`

The package also uses:

* TinyXML2 — XML configuration parsing
* WebSocket++ — WebSocket communication
* nlohmann/json — JSON serialization

The required non-ROS libraries can be installed with:

```bash
sudo apt install libtinyxml2-dev
```

```bash
sudo apt install libwebsocketpp-dev
```

```bash
sudo apt install libasio-dev
```

```bash
sudo apt install nlohmann-json3-dev
```

---

# System Architecture

In the complete robot system, the WebSocket bridge can run alongside the nodes responsible for publishing wheel odometry and VectorNav data.

A typical system is:

```text
                         ROS 2 Workspace
                               │
             ┌─────────────────┼─────────────────────┐
             │                 │                     │
             ▼                 ▼                     ▼
       Wheel Odometry      VectorNav         WebSocket Bridge
           Node              Node                  Node
             │                 │                     │
             │                 ├── /vectornav/pose   │
             │                 │                     │
             │                 ├── /vectornav/imu    │
             │                 │                     │
             │                 └── /vectornav/gnss   │
             │                                       │
             └────────── /odom ──────────────────────┤
                                                     │
                                                     ▼
                                            Build Compact JSON
                                                     │
                                                     ▼
                                             WebSocket Client
                                                     │
                                                     ▼
                                            WebSocket Server
                                                     │
                                                     ▼
                                            External Application
```

The `websocket_bridge` package is independent of the nodes that publish the sensor data. It only requires the expected ROS 2 topics to be available and publishing.

---

# Quick Start

## 1. Build

```bash
cd ~/SEV_ws
colcon build --packages-select websocket_bridge
```

## 2. Source the workspace

```bash
source install/setup.bash
```

## 3. Start the WebSocket server

Start the external WebSocket server that will receive the JSON data.

The server must use the host and port specified in:

```text
config/websocket_bridge.xml
```

The default configuration is:

```text
ws://127.0.0.1:9002
```

## 4. Start the ROS 2 bridge

```bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

## 5. Verify the ROS topics

Confirm that the required topics are available:

```text
/odom
/vectornav/pose
/vectornav/imu
/vectornav/gnss
```

Use:

```bash
ros2 topic list
```

## 6. Verify JSON reception

Check the WebSocket server for incoming compact JSON messages containing the selected robot state, sensor data, latitude, and longitude information.

---

# Summary

The `websocket_bridge` package provides a bridge between ROS 2 and external applications using WebSockets.

The bridge:

1. Subscribes to `/odom`.
2. Subscribes to `/vectornav/pose`.
3. Subscribes to `/vectornav/imu`.
4. Subscribes to `/vectornav/gnss`.
5. Collects geographic latitude and longitude data from GNSS.
6. Stores the latest available data.
7. Packages the required data into a compact JSON message.
8. Connects to a configured WebSocket server.
9. Sends JSON data at the configured publish rate.

Configuration is controlled through:

```text
config/websocket_bridge.xml
```

The package is intended to run as part of a larger ROS 2 system and provides a lightweight interface for making robot state, sensor, and geographic position data available to external applications over a WebSocket connection.
