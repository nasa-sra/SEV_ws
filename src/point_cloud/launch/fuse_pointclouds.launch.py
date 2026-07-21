import cmath
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

CAMERAS = [
    {
        "name": "IntelRealSenseD455",
        "serial_no": "_231622302329",
        "xyz": (0.0, 0.0, 0.0),
        "rpy": (0.0, 0.0, 0.0),
    },
    {
        "name": "IntelRealSenseD455_2",
        "serial_no": "_231622302574",
        "xyz": (-234.367851 / 1000.0, 67.184139 / 1000.0, 0.0 / 1000.0),
        "rpy": (0.0, 0.0, cmath.pi / 2.0),
    },
    {
        "name": "IntelRealSenseD435",
        "serial_no": "_827312071735",
        "xyz": (-98.226089 / 1000.0, 14.740766 / 1000.0, 0.0 / 1000.0),
        "rpy": (cmath.pi, 0.0, cmath.pi / 4.0),
    },
]
PRIMARY_CAMERA = CAMERAS[0]["name"]
GLOBAL_FRAME = "map"


def generate_launch_description():
    ld = LaunchDescription()

    package_share_dir = get_package_share_directory("point_cloud")
    realsense_launch_dir = get_package_share_directory("realsense2_camera")
    input_topics = []

    odometry_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(package_share_dir, "launch", "odometry.launch.py")
        )
    )
    ld.add_action(odometry_launch)

    for cam in CAMERAS:
        name = cam["name"]
        x, y, z = cam["xyz"]
        roll, pitch, yaw = cam["rpy"]

        input_topics.append(f"/{name}/{name}/depth/color/points")

        camera_args = {
            "camera_name": name,
            "camera_namespace": name,
            "serial_no": cam["serial_no"],
            "enable_color": "true",
            "enable_depth": "true",
            "align_depth.enable": "true",
            "pointcloud.enable": "true",
            "accelerate_gpu_with_gsl": "true",
            "decimation_filter.enable": "true",
            "decimation_filter.magnitude": "2",
            "spatial_filter.enable": "true",
            "spatial_filter.holes_fill": "1",
            "temporal_filter.enable": "true",
            "temporal_filter.alpha": "0.4",
            "temporal_filter.delta": "20",
            "HDR_merge.enable": "true",
            "rotation_filter.enable": "true",
            "depth_module_infra_profile": "1280x720x30",
        }
        if name == PRIMARY_CAMERA:
            camera_args.update(
                {
                    "enable_gyro": "true",
                    "enable_accel": "true",
                    "unite_imu_method": "2",
                }
            )

        camera_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(realsense_launch_dir, "launch", "rs_launch.py")
            ),
            launch_arguments=camera_args.items(),
        )
        ld.add_action(camera_launch)

        if name != PRIMARY_CAMERA:
            static_tf = Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                name=f"static_tf_{name}",
                arguments=[
                    "--x",
                    str(x),
                    "--y",
                    str(y),
                    "--z",
                    str(z),
                    "--roll",
                    str(roll),
                    "--pitch",
                    str(pitch),
                    "--yaw",
                    str(yaw),
                    "--frame-id",
                    f"{PRIMARY_CAMERA}_link",
                    "--child-frame-id",
                    f"{name}_link",
                ],
            )
            ld.add_action(static_tf)

    fusion_node = Node(
        package="point_cloud",
        executable="pointcloud_fusion_node",
        name="pointcloud_fusion_node",
        output="screen",
        parameters=[
            {
                "input_topics": input_topics,
                "output_topic": "/fused_pointcloud",
                "target_frame": GLOBAL_FRAME,
                "publish_rate_hz": 10.0,
                "max_cloud_age_seconds": 1.0,
                "use_color": True,
                "voxel_leaf_size": 0.4,
                "accumulate_clouds": True,
                "accumulation_voxel_leaf_size": 0.2,
                "max_accumulated_points": 1000000,
                "qos_reliability": "best_effort",
            }
        ],
    )
    ld.add_action(fusion_node)

    return ld
