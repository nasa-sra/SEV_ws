import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource


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

    for cam in load_cameras():
        name = cam["name"]
        args = {
            "camera_name": name,
            "camera_namespace": name,
            "serial_no": cam["serial_no"],
            # THIS IS VERY IMPORTANT TO MAKE SURE DEPTH INFO IS THERE
            "enable_infra1": "true",
            "enable_infra2": "true",
            "enable_depth": "true",
            "enable_color": "true",
            "align_depth.enable": "true",
            "pointcloud.enable": "false",  # DO NOT USE REALSENSE POINT CLOUDS BECAUSE NVLBOX DOES IT
            "depth_module.emitter_enabled": "1",
            "depth_module.emitter_on_off": "true",
            # tunign performance for jetson or GPU.
            "depth_module.depth_profile": "848x480x30",
            "depth_module.infra_profile": "848x480x30",
            "rgb_camera.color_profile": "848x480x30",
            # apparently it is not best practice to use realsense camera filters, DO NOT ADD THEM IT MAKES IT BAD
        }
        if cam.get("imu"):
            args.update(
                {
                    "enable_gyro": "true",
                    "enable_accel": "true",
                    "unite_imu_method": "2",
                }
            )

        ld.add_action(
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(rs_launch),
                launch_arguments=args.items(),
            )
        )

    return ld
