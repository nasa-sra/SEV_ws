import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
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

    # nvblox is really bad and slow to start so I made this self-expnatory gate
    pose_gate = Node(
        package="fast_point_cloud",
        executable="wait_for_stable_pose.py",
        name="wait_for_stable_pose",
        output="screen",
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "profile",
                default_value="dev",
                description="Performance profile: dev (RTX) or Jetson",
            ),
            DeclareLaunchArgument("use_rviz", default_value="true"),
            DeclareLaunchArgument(
                "ground_constraint",
                default_value="false",
                description="TRUE ON SEV, FALSE ON HANDHELD",
            ),
            DeclareLaunchArgument(
                "imu_fusion",
                default_value="false",
                description="Whether to fuse the D455 IMU into the VIO",
            ),
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                parameters=[{"robot_description": sev_description}],
                output="screen",
            ),
            include("cameras.launch.py"),
            include(
                "visual_slam.launch.py",
                launch_arguments={
                    "ground_constraint": LaunchConfiguration("ground_constraint"),
                    "imu_fusion": LaunchConfiguration("imu_fusion"),
                }.items(),
            ),
            pose_gate,
            RegisterEventHandler(
                OnProcessExit(
                    target_action=pose_gate,
                    on_exit=[
                        include(
                            "nvblox.launch.py",
                            launch_arguments={"profile": profile}.items(),
                        )
                    ],
                )
            ),
            include("surround_view.launch.py"),
            include("rviz.launch.py", condition=IfCondition(use_rviz)),
        ]
    )
