import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch_ros.parameter_descriptions import ParameterValue


def load_cameras():
    path = os.path.join(
        get_package_share_directory("fast_point_cloud"), "config", "cameras.yaml"
    )
    with open(path) as f:
        return yaml.safe_load(f)["cameras"]


def generate_launch_description():
    #  ONLY CAMS THAT ARE IN VSLAM NOT THE BUM CAMS THAT AREN"T
    cameras = [c for c in load_cameras() if c.get("vslam", True)]

    remappings = []
    optical_frames = []
    for i, cam in enumerate(cameras):
        name = cam["name"]
        left, right = 2 * i, 2 * i + 1
        remappings += [
            (
                f"visual_slam/image_{left}",
                f"/{name}/realsense_splitter_node/output/infra_1",
            ),
            (f"visual_slam/camera_info_{left}", f"/{name}/{name}/infra1/camera_info"),
            (
                f"visual_slam/image_{right}",
                f"/{name}/realsense_splitter_node/output/infra_2",
            ),
            (f"visual_slam/camera_info_{right}", f"/{name}/{name}/infra2/camera_info"),
        ]
        optical_frames += [
            f"{name}_infra1_optical_frame",
            f"{name}_infra2_optical_frame",
        ]
    imu_cam = next((c["name"] for c in cameras if c.get("imu")), None)
    if imu_cam:
        remappings.append(("visual_slam/imu", f"/{imu_cam}/{imu_cam}/imu"))
    params = {
        "num_cameras": len(cameras) * 2,
        "min_num_images": 2,
        "rectified_images": True,
        "enable_image_denoising": False,
        "enable_localization_n_mapping": True,
        "enable_imu_fusion": ParameterValue(
            LaunchConfiguration("imu_fusion"), value_type=bool
        ),
        "enable_ground_constraint_in_odometry": ParameterValue(
            LaunchConfiguration("ground_constraint"), value_type=bool
        ),
        "enable_ground_constraint_in_slam": False,
        "map_frame": "map",
        "odom_frame": "odom",
        "base_frame": "base_link",
        "publish_odom_to_base_tf": True,
        "publish_map_to_odom_tf": True,
        "camera_optical_frames": optical_frames,
        "image_qos": "SENSOR_DATA",
    }
    if imu_cam:
        params["imu_frame"] = f"{imu_cam}_imu_optical_frame"
        # these numbers are from nvidia reference
        params.update(
            {
                "gyro_noise_density": 0.000244,
                "gyro_random_walk": 0.000019393,
                "accel_noise_density": 0.001862,
                "accel_random_walk": 0.003,
                "calibration_frequency": 200.0,
            }
        )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "imu_fusion",
                default_value="false",
                description="Whether to fuse the D455 IMU into the VIO",
            ),
            DeclareLaunchArgument(
                "ground_constraint",
                default_value="true",
                description="TRUE ON SEV, FALSE ON HANDHELD",
            ),
            ComposableNodeContainer(
                name="visual_slam_container",
                namespace="",
                package="rclcpp_components",
                executable="component_container_mt",
                composable_node_descriptions=[
                    ComposableNode(
                        package="isaac_ros_visual_slam",
                        plugin="nvidia::isaac_ros::visual_slam::VisualSlamNode",
                        name="visual_slam_node",
                        parameters=[params],
                        remappings=remappings,
                    )
                ],
                output="screen",
            ),
        ]
    )
