from launch import LaunchDescription
from launch_ros.actions import Node

PRIMARY_CAMERA = "IntelRealSenseD455"


def generate_launch_description():
    ld = LaunchDescription()

    imu_filter = Node(
        package="imu_filter_madgwick",
        executable="imu_filter_madgwick_node",
        name="imu_filter",
        output="screen",
        parameters=[
            {
                "use_mag": False,
                "publish_tf": False,
                "world_frame": "enu",
            }
        ],
        remappings=[
            ("imu/data_raw", f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/imu"),
            ("imu/data", f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/imu/filtered"),
        ],
    )
    ld.add_action(imu_filter)

    rgbd_odometry = Node(
        package="rtabmap_odom",
        executable="rgbd_odometry",
        name="rgbd_odometry",
        output="screen",
        parameters=[
            {
                "frame_id": f"{PRIMARY_CAMERA}_link",
                "odom_frame_id": "odom",
                "publish_tf": True,
                "approx_sync": True,
                "approx_sync_max_interval": 0.1,
                "Odom/Strategy": "0",
                "Odom/ResetCountdown": "0",
                "Odom/GuessSmoothingDelay": "0",
                "RGBD/OptimizeMaxError": "0.2",
                "Vis/MinInliers": "10",
                "Vis/CorNNDRRatio": "0.6",
                "wait_imu_to_init": True,
            }
        ],
        remappings=[
            ("rgb/image", f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/color/image_raw"),
            (
                "rgb/camera_info",
                f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/color/camera_info",
            ),
            (
                "depth/image",
                f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/aligned_depth_to_color/image_raw",
            ),
            ("imu", f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/imu/filtered"),
        ],
    )
    ld.add_action(rgbd_odometry)

    rtabmap_slam = Node(
        package="rtabmap_slam",
        executable="rtabmap",
        name="rtabmap",
        output="screen",
        parameters=[
            {
                "frame_id": f"{PRIMARY_CAMERA}_link",
                "odom_frame_id": "odom",
                "map_frame_id": "map",
                "subscribe_depth": True,
                "subscribe_rgb": True,
                "approx_sync": True,
                "database_path": "/tmp/rtabmap.db",
                "RGBD/ProximityBySpace": "true",
                "Reg/Strategy": "1",
                "Icp/VoxelSize": "0.05",
                "Mem/STMSize": "30",
            }
        ],
        remappings=[
            ("rgb/image", f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/color/image_raw"),
            (
                "rgb/camera_info",
                f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/color/camera_info",
            ),
            (
                "depth/image",
                f"/{PRIMARY_CAMERA}/{PRIMARY_CAMERA}/aligned_depth_to_color/image_raw",
            ),
        ],
        arguments=["--delete_db_on_start"],
    )
    ld.add_action(rtabmap_slam)

    return ld
