import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch_ros.substitutions import FindPackageShare


def load_cameras():
    path = os.path.join(
        get_package_share_directory("fast_point_cloud"), "config", "cameras.yaml"
    )
    with open(path) as f:
        return yaml.safe_load(f)["cameras"]


def generate_launch_description():
    cameras = load_cameras()
    pkg = FindPackageShare("fast_point_cloud")
    profile = LaunchConfiguration("profile")
    remappings = []
    for i, cam in enumerate(cameras):
        name = cam["name"]
        remappings += [
            (
                f"camera_{i}/depth/image",
                f"/{name}/{name}/aligned_depth_to_color/image_raw",
            ),
            (
                f"camera_{i}/depth/camera_info",
                f"/{name}/{name}/aligned_depth_to_color/camera_info",
            ),
            (f"camera_{i}/color/image", f"/{name}/{name}/color/image_raw"),
            (f"camera_{i}/color/camera_info", f"/{name}/{name}/color/camera_info"),
        ]

    base_params = PathJoinSubstitution([pkg, "config", "nvblox_base.yaml"])
    profile_params = PathJoinSubstitution(
        [pkg, "config", ["nvblox_", profile, ".yaml"]]
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "profile",
                default_value="dev",
                description="Profile to use for the launch (dev (RTX GPUS), jetson)",
            ),
            ComposableNodeContainer(
                name="nvblox_container",
                namespace="",
                package="rclcpp_components",
                executable="component_container_mt",
                composable_node_descriptions=[
                    ComposableNode(
                        package="isaac_ros_nvblox",
                        plugin="nvidia::isaac_ros::nvblox::NvbloxNode",
                        name="nvblox_node",
                        parameters=[
                            base_params,
                            profile_params,
                            {"num_cameras": len(cameras)},
                        ],
                        remappings=remappings,
                    )
                ],
                output="screen",
            ),
        ]
    )
