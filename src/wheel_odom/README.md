# wheel_odom

`wheel_odom` is a ROS 2 Jazzy package that receives wheel odometry data over UDP and publishes it as a `nav_msgs/msg/Odometry` topic.

This package, and by consequence, this readme is frequently changing right now, if there are any inaccuracies or errors, let me know!

The package is intended to bridge SEV cabin computer and the Jetsons used for SEV Autonomous

---

## Features

- Receives wheel odometry data over UDP.
- Publishes ROS 2 `nav_msgs/msg/Odometry` messages.
- Uses a dedicated thread for non-blocking UDP reception.
- Publishes odometry at a configurable timer interval.
- Designed for low-latency communication with external hardware.

---

## Package Structure

```
wheel_odom/
├── include/
│   └── odomPublisher.hpp
|   └── SocketAddress.h
|   └── UdpSocket.h
├── src/
│   ├── odomPublisher.cpp
|   └── SocketAddress.cpp
|   └── standalone_odomPublisher.cpp
|   └── udp_sender
|   └── udp_sender_test.cpp
├── launch/
│   └── wheel_odom.launch.py
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## How It Works

1. The node creates and binds a UDP socket to a local port.
2. A dedicated listener thread waits for incoming UDP packets.
3. Received packets are decoded into an internal odometry representation.
4. A ROS 2 timer periodically publishes the latest available odometry as a `nav_msgs/msg/Odometry` message.

The networking and publishing threads are separated so that waiting for UDP packets does not block ROS callbacks.

---

## Default Configuration

| Parameter | Default |
|-----------|---------|
| UDP listen port | `12345` |
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

If the executable is installed, it can also be run directly:

```bash
ros2 run wheel_odom odom_publisher
```

---

## Expected UDP Behavior

The node acts as a UDP receiver.

After launching successfully, it binds to the configured local port and waits for incoming packets. No connection is required because UDP is connectionless.

If no packets are received, the node continues running and periodically publishes the most recently available odometry (or default values until valid data has been received).

---

## Topics

### Published

| Topic | Type |
|--------|------|
| `odom` | `nav_msgs/msg/Odometry` |

---

## Threading Model

The node consists of two primary execution paths:

- **UDP listener thread**
  - Blocks while waiting for incoming UDP packets.
  - Updates the latest odometry data.

- **ROS timer callback**
  - Executes at a fixed interval.
  - Publishes the most recent odometry message.

A mutex protects shared odometry data between these threads.

---

## Dependencies

- ROS 2 Jazzy
- `rclcpp`
- `nav_msgs`
- `geometry_msgs`
- C++20 (for `std::jthread` and `std::stop_token`)
- UDP socket library (`UdpSocket`)

---

## Future Improvements

Potential enhancements include:

- Configurable UDP port through ROS parameters.
- Configurable publish frequency.
- Packet validation (checksum or magic bytes).
- Packet timestamping.
- Diagnostics and packet statistics.
- TF transform broadcasting.
- Covariance estimation.
- Automatic reconnection and socket health monitoring.
- Unit and integration tests.

---

## License

Apache-2.0
