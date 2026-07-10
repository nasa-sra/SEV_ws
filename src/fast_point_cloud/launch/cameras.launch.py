import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def load_cameras():
    path = os.path.join(
        get_package_share_directory("fast_point_cloud"), "config", "cameras.yaml"
    )
    with open(path) as f:
        return yaml.safe_load(f)["cameras"]


def generate_launch_description():
    ld = LaunchDescription()
    rs_launch = os.path.join(
        get_package_share_directory("realsense2_camera"), "launch", "rs_launch.py"
    )
    share = get_package_share_directory("fast_point_cloud")
    flashing_config = os.path.join(share, "config", "realsense_flashing.yaml")
    emitter_on_config = os.path.join(share, "config", "realsense_emitter_on.yaml")

    splitter_nodes = []

    for idx, cam in enumerate(load_cameras()):
        name = cam["name"]
        uses_vslam = cam.get("vslam", True)
        stereo_profile = cam.get("stereo_profile", "640x480x30")
        color_profile = cam.get("color_profile", "640x480x30")
        args = {
            "camera_name": name,
            "camera_namespace": name,
            "serial_no": cam["serial_no"],
            "config_file": flashing_config if uses_vslam else emitter_on_config,
            "initial_reset": "true",
            "enable_infra1": "true" if uses_vslam else "false",
            "enable_infra2": "true" if uses_vslam else "false",
            "enable_depth": "true",
            "enable_color": "true",
            "align_depth.enable": "false",
            "pointcloud.enable": "false",  # nvblox builds its own clouds DO NOT ENABLE
            "depth_module.depth_profile": stereo_profile,
            "depth_module.infra_profile": stereo_profile,
            "rgb_camera.color_profile": color_profile,
            "enable_sync": "false",  # very confusing at first but this was the only way to get the cameras to start streaming
        }
        if cam.get("imu"):
            args.update(
                {
                    "enable_gyro": "true",
                    "enable_accel": "true",
                    "unite_imu_method": "2",
                }
            )

        # staggering startups so the realsense cameras don't tweak out at once and crash
        ld.add_action(
            TimerAction(
                period=idx * 10.0,
                actions=[
                    IncludeLaunchDescription(
                        PythonLaunchDescriptionSource(rs_launch),
                        launch_arguments=args.items(),
                    )
                ],
            )
        )

        if not uses_vslam:
            continue
        splitter_nodes.append(
            ComposableNode(
                namespace=name,
                name="realsense_splitter_node",
                package="realsense_splitter",
                plugin="nvblox::RealsenseSplitterNode",
                parameters=[{"input_qos": "SENSOR_DATA", "output_qos": "SENSOR_DATA"}],
                remappings=[
                    ("input/infra_1", f"/{name}/{name}/infra1/image_rect_raw"),
                    ("input/infra_1_metadata", f"/{name}/{name}/infra1/metadata"),
                    ("input/infra_2", f"/{name}/{name}/infra2/image_rect_raw"),
                    ("input/infra_2_metadata", f"/{name}/{name}/infra2/metadata"),
                    ("input/depth", f"/{name}/{name}/depth/image_rect_raw"),
                    ("input/depth_metadata", f"/{name}/{name}/depth/metadata"),
                    ("input/pointcloud", f"/{name}/{name}/depth/color/points"),
                    ("input/pointcloud_metadata", f"/{name}/{name}/depth/metadata"),
                ],
            )
        )

    ld.add_action(
        ComposableNodeContainer(
            name="splitter_container",
            namespace="",
            package="rclcpp_components",
            executable="component_container_mt",
            composable_node_descriptions=splitter_nodes,
            output="screen",
        )
    )

    cameras = load_cameras()
    vslam_cams = [c["name"] for c in cameras if c.get("vslam", True)]
    depth_only_cams = [c["name"] for c in cameras if not c.get("vslam", True)]
    if cameras:
        ld.add_action(
            Node(
                package="fast_point_cloud",
                executable="camera_watchdog.py",
                name="camera_watchdog",
                parameters=[
                    {
                        "vslam_cameras": vslam_cams or [""],
                        "depth_only_cameras": depth_only_cams or [""],
                    }
                ],
                output="screen",
            )
        )

    return ld
