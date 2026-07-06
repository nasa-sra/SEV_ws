import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg = FindPackageShare("fast_point_cloud")
    launch_directory = os.path.join(
        get_package_share_directory("fast_point_cloud"), "launch"
    )

    profile = LaunchConfiguration("profile")
    use_rviz = LaunchConfiguration("use_rviz")

    sev_description = ParameterValue(
        Command(
            ["xacro ", PathJoinSubstitution([pkg, "description", "sev.urdf.xacro"])]
        ),
        value_type=str,
    )

    def include(name, **kwargs):
        return IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_directory, name)),
            **kwargs
        )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "profile",
                default_value="dev",
                description="Profile to use for the launch (dev (RTX GPUS), jetson)",
            ),
            DeclareLaunchArgument("use_rviz", default_value="true"),
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                parameters=[{"robot_description": sev_description}],
                output="screen",
            ),
            include("cameras.launch.py"),
            include("visual_slam.launch.py"),
            include("nvblox.launch.py", launch_arguments={"profile": profile}.items()),
            include("rviz.launch.py", condition=IfCondition(use_rviz)),
        ]
    )
