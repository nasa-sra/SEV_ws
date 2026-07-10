import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    params = os.path.join(
        get_package_share_directory("fast_point_cloud"),
        "config",
        "surround_view.yaml",
    )
    return LaunchDescription(
        [
            Node(
                package="fast_point_cloud",
                executable="surround_view_node.py",
                name="surround_view",
                parameters=[params],
                output="screen",
            )
        ]
    )
