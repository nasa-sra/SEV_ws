import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def load_cameras():
    path = os.path.join(
        get_package_share_directory("fast_point_cloud"), "config", "cameras.yaml"
    )
    with open(path) as f:
        return yaml.safe_load(f)["cameras"]


def generate_launch_description():
    cameras = load_cameras()

    remappings = []
    optical_frames = []
    for i, cam in enumerate(cameras):
        name = cam["name"]
        left, right = (
            2 * i,
            2 * i + 1,
        )  # very clever math yk, i spent a lot of time on this
        remappings += [
            (f"visual_slam/image_{left}", f"/{name}/{name}/infra1/image_rect_raw"),
            (f"visual_slam/camera_info_{left}", f"/{name}/{name}/infra1/camera_info"),
            (f"visual_slam/image_{right}", f"/{name}/{name}/infra2/image_rect_raw"),
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
        "enable_localization_n_mapping": True,  # apprently this is important for vslam to work
        "enable_imu_fusion": bool(imu_cam),
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
        # TODO -> from gemini -> set D455 IMU noise densities (gyro/accel noise + random walk) from the datasheet for best VIO

    return LaunchDescription(
        [
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
            )
        ]
    )
