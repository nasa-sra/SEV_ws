# WebSocket Bridge

The `websocket_bridge` package is a ROS 2 node that collects robot state and sensor data from multiple ROS 2 topics, packages the data into JSON, and sends the JSON data to an external application over a WebSocket connection.

The bridge subscribes to:

* `/odom`
* `/vectornav/pose`
* `/vectornav/imu`

The latest data from these topics is stored by the bridge and packaged into a JSON message that is transmitted to a WebSocket server at a configurable rate.

---

## Overview

The overall data flow is:

```text
                         ROS 2
                           │
            ┌──────────────┼──────────────┐
            │              │              │
            ▼              ▼              ▼
         /odom      /vectornav/pose   /vectornav/imu
            │              │              │
            │              │              │
            └──────────────┼──────────────┘
                           │
                           ▼
                  websocket_bridge
                           │
                           │ Build JSON
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

The WebSocket bridge acts as a **WebSocket client**. It connects to a WebSocket server that must already be running.

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
* Storing the latest received messages.
* Loading the XML configuration.
* Building the JSON payload.
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

The bridge subscribes to three ROS 2 topics.

---

## `/odom`

Message type:

```text
nav_msgs/msg/Odometry
```

This topic provides odometry information.

The bridge can use information such as:

* Position.
* Orientation.
* Linear velocity.
* Angular velocity.
* Pose covariance.
* Twist covariance.

The message structure is:

```text
Odometry
├── header
├── child_frame_id
├── pose
│   ├── pose
│   │   ├── position
│   │   └── orientation
│   └── covariance
└── twist
    ├── twist
    │   ├── linear
    │   └── angular
    └── covariance
```

---

## `/vectornav/pose`

Message type:

```text
geometry_msgs/msg/PoseWithCovarianceStamped
```

This topic is expected to be published by the VectorNav ROS 2 node.

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

The bridge can use:

* Position.
* Orientation.
* Pose covariance.
* Header information.

Because the message type is `PoseWithCovarianceStamped`, position data is accessed through the additional `pose` layer.

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

The bridge can use information such as:

* Orientation.
* Angular velocity.
* Linear acceleration.
* Orientation covariance.
* Angular velocity covariance.
* Linear acceleration covariance.

---

# WebSocket Configuration

The bridge uses the following XML configuration file:

```text
config/websocket_bridge.xml
```

The exact XML structure must match the configuration parser implemented in `websocket_bridge.cpp`.

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

The publish rate controls how frequently the bridge attempts to send JSON data.

For example:

```xml
<publish_rate>10.0</publish_rate>
```

means the bridge operates at approximately:

```text
10 Hz
```

or:

```text
10 messages per second
```

A configuration of:

```xml
<publish_rate>1.0</publish_rate>
```

would result in approximately one message per second.

---

# WebSocket Connection

The bridge acts as a **WebSocket client**.

For example, with the default configuration:

```text
ws://127.0.0.1:9002
```

the bridge attempts to connect to a WebSocket server running on:

```text
IP address: 127.0.0.1
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

This means that the bridge attempted to connect but no WebSocket server accepted the connection.

---

# JSON Data

The bridge packages ROS 2 data into a JSON object before sending it over the WebSocket connection.

The exact JSON structure depends on the implementation of `buildJson()` in:

```text
src/websocket_bridge.cpp
```

A typical JSON payload may look like:

```json
{
    "odom": {
        "position": {
            "x": 1.23,
            "y": 4.56,
            "z": 0.0
        },
        "orientation": {
            "x": 0.0,
            "y": 0.0,
            "z": 0.707,
            "w": 0.707
        }
    },
    "vectornav_pose": {
        "position": {
            "x": 1.25,
            "y": 4.58,
            "z": 0.02
        },
        "orientation": {
            "x": 0.0,
            "y": 0.0,
            "z": 0.707,
            "w": 0.707
        }
    },
    "imu": {
        "linear_acceleration": {
            "x": 0.1,
            "y": 0.2,
            "z": 9.81
        },
        "angular_velocity": {
            "x": 0.01,
            "y": 0.02,
            "z": 0.03
        }
    }
}
```

The JSON is sent as a WebSocket text message.

The receiving application should parse the incoming WebSocket message as standard JSON.

---

# Data Flow

The bridge stores the latest message received from each subscribed topic.

The general process is:

```text
/odom publishes
      │
      ▼
odomCallback()
      │
      ▼
Store latest odometry


/vectornav/pose publishes
      │
      ▼
poseCallback()
      │
      ▼
Store latest VectorNav pose


/vectornav/imu publishes
      │
      ▼
imuCallback()
      │
      ▼
Store latest IMU data


Required data available
      │
      ▼
buildJson()
      │
      ▼
Send JSON over WebSocket
```

The bridge uses the latest available data from each ROS topic when creating the JSON message.

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

If CMake or build configuration issues occur, clean the package build:

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

with the following contents:

```python
import asyncio
import json
import websockets


HOST = "127.0.0.1"
PORT = 9002


async def handle_client(websocket):

    print("Client connected!")

    try:

        async for message in websocket:

            print("\nReceived WebSocket message:")

            try:

                data = json.loads(message)

                print(json.dumps(data, indent=4))

            except json.JSONDecodeError:

                print("Received non-JSON message:")
                print(message)

    except websockets.exceptions.ConnectionClosed:

        print("Client disconnected")


async def main():

    print("Starting WebSocket test server")
    print(f"Listening on ws://{HOST}:{PORT}")

    async with websockets.serve(
        handle_client,
        HOST,
        PORT
    ):

        print("Server is running...")
        print("Waiting for WebSocket connection...\n")

        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
```

Run the test server:

```bash
python3 websocket_test_server.py
```

You should see:

```text
Starting WebSocket test server
Listening on ws://127.0.0.1:9002
Server is running...
Waiting for WebSocket connection...
```

In another terminal, start the bridge:

```bash
cd ~/SEV_ws
source install/setup.bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

If the connection is successful, the test server should display:

```text
Client connected!
```

Once the required ROS topics are publishing, the test server should begin printing the received JSON messages.

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

To list all active ROS 2 topics:

```bash
ros2 topic list
```

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

If the ROS topics are not publishing, the bridge will not have new sensor data to package.

---

## VectorNav pose type mismatch

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

## Configuration file not found

The bridge expects the XML configuration to be installed with the package.

After building, verify that the file exists:

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

If the file is missing, verify that the `config` directory is installed by `CMakeLists.txt`:

```cmake
install(
    DIRECTORY
        config
        launch
    DESTINATION share/${PROJECT_NAME}
)
```

Then rebuild:

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
             ┌─────────────────┼─────────────────┐
             │                 │                 │
             ▼                 ▼                 ▼
       Wheel Odometry      VectorNav       WebSocket Bridge
           Node              Node                Node
             │                 │                  │
             │                 ├── /vectornav/   │
             │                 │    pose         │
             │                 │                  │
             │                 └── /vectornav/   │
             │                      imu          │
             │                                    │
             └────── /odom ──────────────────────┤
                                                  │
                                                  ▼
                                           Build JSON
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

The `websocket_bridge` package is independent of the nodes that publish the sensor data. It only requires the expected ROS 2 topics to be available and publishing messages.

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

Confirm that the following topics are available:

```text
/odom
/vectornav/pose
/vectornav/imu
```

Use:

```bash
ros2 topic list
```

## 6. Verify JSON reception

Check the WebSocket server for incoming JSON messages.

---

# Summary

The `websocket_bridge` package provides a bridge between ROS 2 and external applications using WebSockets.

The bridge:

1. Subscribes to `/odom`.
2. Subscribes to `/vectornav/pose`.
3. Subscribes to `/vectornav/imu`.
4. Stores the latest received sensor data.
5. Packages the data into JSON.
6. Connects to a configured WebSocket server.
7. Sends the JSON data at the configured publish rate.

Configuration is controlled through:

```text
config/websocket_bridge.xml
```

The package is intended to run as part of a larger ROS 2 system and provides a simple interface for making robot state and sensor data available to external applications over a WebSocket connection.
