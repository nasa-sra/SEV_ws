# point_cloud

Merges point clouds from N RealSense cameras into a single, spatially accurate, growing map that moves with the camera rig.

## Pipeline overview

```text
fuse_pointclouds.launch.py
├── odometry.launch.py
│   ├── imu_filter (imu_filter_madgwick_node)   raw IMU -> orientation-tagged IMU
│   ├── rgbd_odometry (rtabmap_odom)            odom -> <PRIMARY_CAMERA>_link
│   └── rtabmap (rtabmap_slam)                  map -> odom  (loop closure / drift correction)
├── per-camera realsense2_camera nodes (rs_launch.py)
├── per-secondary-camera static_transform_publisher
└── pointcloud_fusion_node                      subscribes to all camera clouds, publishes /fused_pointcloud
```

`fuse_pointclouds.launch.py` is the single entry point. Do not launch `odometry.launch.py` standalone alongside it — that starts duplicate `rgbd_odometry`/`rtabmap` nodes with the same names, which corrupts TF.

## TF tree

```text
map --(rtabmap, loop-closure corrected)--> odom --(rgbd_odometry)--> <PRIMARY_CAMERA>_link --(static, rigid mount offset)--> <secondary_camera>_link
```

Only the primary camera (`CAMERAS[0]` in `fuse_pointclouds.launch.py`) is tracked by visual odometry. Any additional cameras are assumed to be rigidly bolted to the same rig, so their pose is a fixed static transform relative to `PRIMARY_CAMERA_link`, not relative to `map`. `target_frame` for the fusion node is `map` — the SLAM-corrected, loop-closure frame — not `odom` or `world`.

## Why an IMU filter is in the loop

The D455 IMU only outputs raw gyro + accelerometer samples; the `sensor_msgs/Imu.orientation` field is never set by the camera itself. `rgbd_odometry`'s `wait_imu_to_init` requires a message with orientation populated, so `imu_filter_madgwick_node` sits between the camera's raw `/imu` topic and `rgbd_odometry`, fusing gyro+accel into an orientation estimate (`use_mag: false`, since the D455 has no magnetometer).

Roll and pitch are well-observed by this filter, because the accelerometer's gravity vector gives an absolute reference. Yaw is not — it is only gyro-integrated and would drift if used alone. In this pipeline that's not an issue: the IMU is only an initializer/motion-prior for `rgbd_odometry`, not the source of truth for heading. The camera's visual feature tracking determines yaw, and `rtabmap_slam`'s loop closure corrects accumulated drift (including yaw) on top of that.

## Files

### `launch/fuse_pointclouds.launch.py`

Top-level launch file. Defines `CAMERAS` (each entry: `name`, RealSense `serial_no`, and `xyz`/`rpy` mount offset relative to the primary camera), starts `odometry.launch.py`, starts one `realsense2_camera` node per camera (IMU streams enabled only for the primary camera), starts a static transform for each non-primary camera, and starts `pointcloud_fusion_node` with the list of camera point cloud topics as `input_topics`.

To add a camera: append an entry to `CAMERAS` with its real serial number and its `xyz`/`rpy` offset measured relative to the primary camera's body frame.

### `launch/odometry.launch.py`

Starts the IMU filter, `rgbd_odometry`, and `rtabmap` (SLAM). Included by `fuse_pointclouds.launch.py` — not meant to be run standalone in the same session as it.

`approx_sync_max_interval` on `rgbd_odometry` is `0.1`s; a tighter value caused frame registration to fail intermittently. `Vis/MinInliers` / `Vis/CorNNDRRatio` control how strict visual feature matching is before a frame-to-frame transform is accepted.

### `include/point_cloud/point_cloud_fusion_node.hpp` / `src/point_cloud_fusion_node.cpp`

`PointCloudFusionNode`:

- **`cloudCallback`** — stores the latest message per input topic (`latest_clouds_`), guarded by `buffer_mutex_`.
- **`fusionTimerCallback`** — runs on a timer at `publish_rate_hz_`. Snapshots `latest_clouds_`, drops any cloud older than `max_cloud_age_seconds_`, transforms each surviving cloud into `target_frame_` via `lookupTransformWithFallback`, and hands the transformed clouds to `fuseAndPublish`.
- **`lookupTransformWithFallback`** — looks up `source_frame -> target_frame_` at the message's own timestamp; if that fails (e.g. TF not caught up yet), retries with `tf2::TimePointZero` (latest available transform); returns `std::nullopt` if both fail.
- **`fuseAndPublish<PointT>`** — concatenates the transformed clouds into one PCL cloud, optionally voxel-downsamples it (`voxel_leaf_size_`), then either publishes it directly or merges it into the running accumulated cloud (`accumulatedCloud<PointT>()`), compacting the accumulator with `accumulation_voxel_leaf_size_` once it exceeds `max_accumulated_points_`. Templated on `pcl::PointXYZ` or `pcl::PointXYZRGB` depending on `use_color_`.
- **`accumulatedCloud<PointT>`** — returns a reference to `acc_xyz_` or `acc_xyzrgb_` (explicit template specializations).
- **`publishCloud<PointT>`** — converts a PCL cloud to `sensor_msgs::msg::PointCloud2`, stamps it with `target_frame_` and the current time, and publishes it.
- **`clearAccumulatedCloud`** — clears both accumulators; exposed as the `~/clear_accumulated_cloud` service.

### `src/point_cloud_fusion_main.cpp`

Standard `rclcpp` entry point; constructs and spins `PointCloudFusionNode`.

## Node parameters (`pointcloud_fusion_node`)

| Parameter | Default | Meaning |
| --- | --- | --- |
| `input_topics` | `[]` | Point cloud topics to subscribe to and fuse |
| `output_topic` | `/fused_point_cloud` | Topic the fused cloud is published on |
| `target_frame` | `world` | Frame all input clouds are transformed into before fusing (set to `map` in `fuse_pointclouds.launch.py`) |
| `publish_rate_hz` | `10.0` | Fusion/publish timer rate |
| `max_cloud_age_seconds` | `1.0` | Clouds older than this (by header stamp vs. now) are dropped |
| `use_color` | `true` | Selects `pcl::PointXYZRGB` vs `pcl::PointXYZ` |
| `voxel_leaf_size` | `0.0` | Voxel grid leaf size applied to each fused frame before accumulation (`0` disables) |
| `qos_reliability` | `reliable` | `reliable` or `best_effort` for the input subscriptions |
| `accumulate_clouds` | `false` | If true, build a persistent map instead of publishing only the latest fused frame |
| `accumulation_voxel_leaf_size` | `0.05` | Leaf size used to compact the accumulator once it exceeds `max_accumulated_points` |
| `max_accumulated_points` | `5000000` | Point count threshold that triggers compaction |

## Services

- `~/clear_accumulated_cloud` (`std_srvs/Empty`) — clears the accumulated map without restarting the node.

## Running

```bash
ros2 launch point_cloud fuse_pointclouds.launch.py
```

Move the camera slowly and deliberately, especially early on — RGB-D visual odometry can lose tracking (and therefore TF accuracy) under fast motion, particularly before the IMU/visual fusion has stabilized.

## Useful RViz2 setup

- Fixed Frame: `map`
- `TF` display, to confirm `map -> odom -> <PRIMARY_CAMERA>_link` is updating as the camera moves
- `PointCloud2` on `/fused_pointcloud` — the final output
- `Odometry` on `/odom` (rgbd_odometry's raw output) to sanity-check the pose trail against TF
- `PointCloud2` on `/<camera>/<camera>/depth/color/points` to inspect a single camera's raw colorized cloud directly, useful when diagnosing depth/color alignment

## Dependencies

`ros-jazzy-rtabmap-ros`, `ros-jazzy-imu-filter-madgwick`, in addition to the packages declared in `package.xml`.
