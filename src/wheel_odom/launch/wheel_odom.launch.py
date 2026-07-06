from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():

    return LaunchDescription([
        Node(
            package='wheel_odom',
            executable='odom_publisher',
            name='odomPublisher',
            output='screen'
        )
    ])