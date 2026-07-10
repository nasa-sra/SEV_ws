from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    port = LaunchConfiguration("port")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "port",
                default_value="8765",
                description="Foxglove WebSocket port",
            ),
            # copied
            Node(
                package="foxglove_bridge",
                executable="foxglove_bridge",
                name="foxglove_bridge",
                output="screen",
                parameters=[
                    {
                        "port": port,
                        "address": "0.0.0.0",
                        "send_buffer_limit": 100000000,
                        "max_qos_depth": 10,
                    }
                ],
            ),
        ]
    )
