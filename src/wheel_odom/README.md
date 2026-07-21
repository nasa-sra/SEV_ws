# wheel_odom

`wheel_odom` is a ROS 2 Jazzy package that receives wheel odometry data over UDP and publishes it as a `nav_msgs/msg/Odometry` topic.

This package, and by consequence this README, is under active development. If you encounter inaccuracies or outdated information, please update the documentation accordingly.

The package is intended to bridge the SEV cabin computer and the NVIDIA Jetson systems used for SEV Autonomous.

---

## Features

- Receives wheel odometry data over UDP.
- Publishes ROS 2 `nav_msgs/msg/Odometry` messages.
- Uses a dedicated thread for non-blocking UDP reception.
- Publishes odometry at a configurable timer interval.
- Configurable UDP listening port through an XML configuration file.
- Designed for low-latency communication with external hardware.

---

## Package Structure

```text
wheel_odom/
├── config/
│   └── udp_config.xml
├── include/
│   ├── odomPublisher.hpp
│   ├── SocketAddress.h
│   └── UdpSocket.h
├── launch/
│   └── wheel_odom.launch.py
├── src/
│   ├── odomPublisher.cpp
│   ├── standalone_odomPublisher.cpp
│   ├── SocketAddress.cpp
│   ├── UdpSocket.cpp
│   ├── udp_sender
│   └── udp_sender_test.cpp
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## How It Works

1. The node reads the UDP listening port from an XML configuration file.
2. A UDP socket is created and bound to the configured local port.
3. A dedicated listener thread waits for incoming UDP packets.
4. Received packets are decoded into an internal odometry representation.
5. A ROS 2 timer periodically publishes the latest available odometry as a `nav_msgs/msg/Odometry` message.

Networking and publishing are handled on separate execution paths so that waiting for UDP packets does not block ROS callbacks.

---

## Configuration

The UDP listening port is configured using the XML file:

```text
config/udp_config.xml
```

Example:

```xml
<?xml version="1.0"?>
<config>
    <udp_port>8324</udp_port>
</config>
```

During startup, the node reads this file and binds its UDP socket to the configured port.

If the configuration file

- cannot be found,
- contains invalid XML,
- is missing the `<config>` element,
- is missing the `<udp_port>` element, or
- contains an invalid port number,

the node will terminate with an error.

Any external application transmitting wheel odometry must send UDP packets to the same port specified in this file.

---

## Default Configuration

| Parameter | Default |
|-----------|---------|
| UDP listen port | Defined in `config/udp_config.xml` |
| ROS topic | `odom` |
| Publish message type | `nav_msgs/msg/Odometry` |
| Publish period | `500 ms` |

---

## Building

From the workspace root:

```bash
colcon build --packages-select wheel_odom
source install/setup.bash
```

---

## Running

Launch the node:

```bash
ros2 launch wheel_odom wheel_odom.launch.py
```

Alternatively, run the executable directly:

```bash
ros2 run wheel_odom odom_publisher
```

Before launching, ensure that `config/udp_config.xml` contains the UDP port expected by the transmitting application.

Example:

```xml
<config>
    <udp_port>0324</udp_port>
</config>
```

The sender and receiver **must use the same UDP port** for communication.

---

## Expected UDP Behavior

The node acts solely as a UDP receiver.

After launching successfully, it binds to the configured local UDP port and waits for incoming packets. Since UDP is connectionless, no handshake or connection establishment is required.

If no packets are received, the node continues running and periodically publishes the most recently received odometry message (or default-initialized values until valid data is received).

Incoming packets are expected to contain **17 IEEE 754 single-precision floating-point values (68 bytes total)**.

Packets of any other size are rejected.

---

## Topics

### Published

| Topic | Type |
|--------|------|
| `odom` | `nav_msgs/msg/Odometry` |

---

## Threading Model

The node consists of two primary execution paths.

### UDP Listener Thread

- Waits for incoming UDP packets.
- Decodes received packet data.
- Updates the latest odometry message.

### ROS Timer Callback

- Executes at a fixed interval.
- Publishes the latest available odometry message.

A mutex protects shared odometry data between these execution paths.

---

## Dependencies

- ROS 2 Jazzy
- `rclcpp`
- `nav_msgs`
- `geometry_msgs`
- `tf2`
- `tf2_ros`
- `tf2_geometry_msgs`
- `ament_index_cpp`
- `tinyxml2`
- C++20 (for `std::jthread` and `std::stop_token`)
- UDP socket library (`UdpSocket`)

---



## License

Apache-2.0
