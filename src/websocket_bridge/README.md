# WebSocket Bridge

A ROS 2 package that collects robot state data from multiple ROS topics, packages the latest data into a JSON message, and transmits the JSON data to a remote WebSocket server.

The bridge is designed to provide a simple interface between a ROS 2 system and an external application that communicates over WebSockets.

---

## Overview

The `websocket_bridge` node subscribes to three ROS 2 topics:

* `/odom`
* `/vectornav/pose`
* `/vectornav/imu`

The latest message received from each topic is stored by the bridge. At the configured publish rate, the bridge packages the latest available data into a JSON object and sends it to a WebSocket server.

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
│   └── websocket_config.xml
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

Contains the `main()` function and starts the ROS 2 node.

It initializes ROS 2, creates the `WebsocketBridge` node, spins the node, and shuts down ROS 2 when the process exits.

---

### `src/websocket_bridge.cpp`

Contains the main implementation of the WebSocket bridge.

Responsibilities include:

* Creating ROS 2 subscribers
* Receiving `/odom`
* Receiving `/vectornav/pose`
* Receiving `/vectornav/imu`
* Storing the latest received messages
* Loading the XML configuration
* Building the JSON payload
* Connecting to the WebSocket server
* Sending JSON data over the WebSocket

---

### `include/websocket_bridge.hpp`

Contains the `WebsocketBridge` class declaration.

This includes:

* ROS 2 subscriber declarations
* ROS 2 message storage
* Callback declarations
* WebSocket client declarations
* JSON-building functions
* Thread synchronization
* Configuration variables

---

### `config/websocket_config.xml`

Contains the WebSocket connection and bridge configuration.

The XML file is installed with the ROS 2 package and loaded at runtime.

---

### `launch/websocket_bridge.launch.py`

Launches the `websocket_bridge` ROS 2 node.

The node loads its configuration directly from the installed XML configuration file.

---

# ROS 2 Topics

The bridge subscribes to the following topics.

## `/odom`

Message type:

```text
nav_msgs/msg/Odometry
```

The bridge uses the odometry message to obtain robot position, orientation, and other available odometry information.

The message structure includes:

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

The message structure includes:

```text
PoseWithCovarianceStamped
├── header
└── pose
    ├── pose
    │   ├── position
    │   └── orientation
    └── covariance
```

The bridge uses the VectorNav position and orientation data and can also include the covariance information in the JSON payload.

---

## `/vectornav/imu`

Message type:

```text
sensor_msgs/msg/Imu
```

This topic is expected to be published by the VectorNav ROS 2 node.

The bridge uses IMU information such as:

* Linear acceleration
* Angular velocity
* Orientation
* Covariance information, if included in the JSON implementation

---

# WebSocket Configuration

The bridge uses an XML configuration file located at:

```text
config/websocket_config.xml
```

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

The exact XML structure should match the configuration parser implemented in `websocket_bridge.cpp`.

## Host

The host specifies the IP address of the WebSocket server.

For a WebSocket server running on the same computer:

```xml
<host>127.0.0.1</host>
```

For a server running on another computer, replace this with the server's IP address.

For example:

```xml
<host>192.168.0.100</host>
```

---

## Port

The port specifies which port the WebSocket server is listening on.

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
10 messages per second
```

or:

```text
10 Hz
```

A value of:

```xml
<publish_rate>1.0</publish_rate>
```

would result in approximately:

```text
1 message per second
```

---

# WebSocket Connection

The bridge acts as a **WebSocket client**.

It connects to the server configured in:

```text
config/websocket_config.xml
```

For example:

```text
ws://127.0.0.1:9002
```

The bridge does not create a WebSocket server.

Therefore, another application must be running as the WebSocket server.

The connection architecture is:

```text
websocket_bridge
      │
      │ WebSocket client
      │
      ▼
ws://127.0.0.1:9002
      │
      │ WebSocket server
      ▼
External Application
```

If no server is listening on the configured host and port, the bridge will report a connection error such as:

```text
Connection refused
```

This means the bridge could not establish a connection to the WebSocket server.

---

# JSON Data

The bridge packages the ROS 2 data into JSON before sending it over the WebSocket connection.

The exact contents depend on the implementation of `buildJson()`.

A typical payload may look similar to:

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

The receiving application should parse the message as standard JSON.

---

# Message Availability

The bridge stores the latest message received from each ROS topic.

The bridge requires the necessary ROS messages to have been received before it can construct a complete JSON message.

The expected workflow is:

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


All required data available
       │
       ▼
buildJson()
       │
       ▼
WebSocket send
```

If one or more required topics have not published any messages yet, the bridge may wait until the required data becomes available.

---

# Building the Package

From the ROS 2 workspace:

```bash
cd ~/SEV_ws
```

Build only the WebSocket bridge:

```bash
colcon build --packages-select websocket_bridge
```

After building, source the workspace:

```bash
source install/setup.bash
```

If the package has recently been changed and CMake configuration problems occur, clean the package build:

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

The recommended way to run the bridge is:

```bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

You should see output similar to:

```text
[INFO] [websocket_bridge]: Starting WebSocket Bridge
[INFO] [websocket_bridge]: Loading configuration from:
.../install/websocket_bridge/share/websocket_bridge/config/websocket_config.xml
[INFO] [websocket_bridge]: WebSocket URI: ws://127.0.0.1:9002
[INFO] [websocket_bridge]: Publish rate: 10.00 Hz
[INFO] [websocket_bridge]: Connecting to WebSocket server: ws://127.0.0.1:9002
```

---

# Testing the WebSocket Connection

A simple Python WebSocket server can be used to test the bridge without running the complete external application.

Install the Python WebSocket package:

```bash
python3 -m pip install --user websockets
```

Create a test server:

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

Run the server:

```bash
python3 websocket_test_server.py
```

Then run the ROS 2 bridge in another terminal:

```bash
cd ~/SEV_ws
source install/setup.bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

If the connection is successful, the test server should display:

```text
Client connected!
```

Once all required ROS topics are publishing, the server should begin printing the received JSON messages.

---

# Testing ROS Topics

Check whether `/odom` is available:

```bash
ros2 topic info /odom
```

Check VectorNav pose:

```bash
ros2 topic info /vectornav/pose
```

Check VectorNav IMU:

```bash
ros2 topic info /vectornav/imu
```

You can also inspect messages directly:

```bash
ros2 topic echo /odom
```

```bash
ros2 topic echo /vectornav/pose
```

```bash
ros2 topic echo /vectornav/imu
```

To see all active topics:

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

This means the WebSocket bridge cannot connect to the configured WebSocket server.

Check that:

1. The WebSocket server is running.
2. The server is listening on the configured IP address.
3. The server is listening on the configured port.
4. The XML configuration matches the server configuration.

For example, if the bridge uses:

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

Then check the ROS topics:

```bash
ros2 topic info /odom
ros2 topic info /vectornav/pose
ros2 topic info /vectornav/imu
```

The bridge needs the required ROS data to be published before it can create a complete JSON payload.

Verify the topics are actively publishing:

```bash
ros2 topic echo /odom
```

```bash
ros2 topic echo /vectornav/pose
```

```bash
ros2 topic echo /vectornav/imu
```

---

## VectorNav pose type mismatch

The VectorNav pose topic used by this bridge is:

```text
geometry_msgs/msg/PoseWithCovarianceStamped
```

The subscriber is therefore configured for:

```cpp
geometry_msgs::msg::PoseWithCovarianceStamped
```

The appropriate header is:

```cpp
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
```

The pose data is accessed through:

```text
pose.pose.pose.position
```

rather than:

```text
pose.pose.position
```

because `PoseWithCovarianceStamped` contains an additional `PoseWithCovariance` layer.

---

## Configuration file not found

The bridge expects the XML configuration to be installed with the package.

After building, the configuration should be located under:

```text
install/websocket_bridge/share/websocket_bridge/config/
```

Verify it exists:

```bash
ls ~/SEV_ws/install/websocket_bridge/share/websocket_bridge/config/
```

If the file is missing, verify that the `config` directory is installed in `CMakeLists.txt`:

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

* TinyXML2 for XML configuration parsing
* WebSocket++ for WebSocket communication
* nlohmann/json for JSON serialization

On Ubuntu, the required non-ROS libraries can be installed with:

```bash
sudo apt install libtinyxml2-dev
```

```bash
sudo apt install libwebsocketpp-dev
```

```bash
sudo apt install libasio-dev
```

For nlohmann/json:

```bash
sudo apt install nlohmann-json3-dev
```

---

# Typical Deployment

In a complete robot system, the WebSocket bridge can run alongside the nodes responsible for publishing odometry and VectorNav data.

A typical system may look like:

```text
ROS 2 System
│
├── Wheel Odometry Node
│      │
│      └── /odom
│
├── VectorNav Node
│      │
│      ├── /vectornav/pose
│      └── /vectornav/imu
│
└── WebSocket Bridge
       │
       ├── Subscribes to /odom
       ├── Subscribes to /vectornav/pose
       ├── Subscribes to /vectornav/imu
       │
       └── Sends JSON
              │
              ▼
       External WebSocket Server
              │
              ▼
       External Application
```

The WebSocket bridge is therefore independent of the nodes that publish the sensor data. It only requires the expected ROS topics to exist and publish messages.

---

# Quick Start

## 1. Build

```bash
cd ~/SEV_ws
colcon build --packages-select websocket_bridge
```

## 2. Source

```bash
source install/setup.bash
```

## 3. Start the WebSocket server

Make sure the external WebSocket server is listening on the host and port configured in:

```text
config/websocket_config.xml
```

For the default configuration:

```text
ws://127.0.0.1:9002
```

## 4. Start the ROS 2 bridge

```bash
ros2 launch websocket_bridge websocket_bridge.launch.py
```

## 5. Verify ROS topics

```bash
ros2 topic list
```

Confirm that the following topics are available:

```text
/odom
/vectornav/pose
/vectornav/imu
```

## 6. Verify JSON reception

Check the WebSocket server or test server for incoming JSON messages.

---

# Summary

The `websocket_bridge` package provides a bridge between ROS 2 and WebSocket-based applications.

It:

1. Subscribes to `/odom`.
2. Subscribes to `/vectornav/pose`.
3. Subscribes to `/vectornav/imu`.
4. Stores the latest sensor data.
5. Converts the data into JSON.
6. Connects to a configured WebSocket server.
7. Sends the JSON data at the configured publish rate.

The package is intended to run as part of a larger ROS 2 system and provides a simple mechanism for making robot state and sensor data available to external applications over a WebSocket connection.
