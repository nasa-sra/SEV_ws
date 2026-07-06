# fast_point_cloud

GPU-accelerated perception for the Lunar SEV. Multiple Intel RealSense
D455 cameras feed two Isaac ROS components:

- **Isaac ROS Visual SLAM (cuVSLAM)** → **pose** (TF `map → odom → base_link`, `/visual_slam/tracking/odometry`)
- **Isaac ROS Nvblox** → **obstacle map** (a 2D ESDF distance slice + 3D TSDF mesh)

This package **only publishes pose and an obstacle map**. It does not do path
planning or avoidance — a downstream consumer reads the ESDF slice / TF for that.

It is the GPU successor to the `point_cloud` package (which used
`rgbd_odometry` + `rtabmap` + a naive cloud accumulator on the CPU).

## Data flow

```text
RealSense × N ─┬─ infra1 + infra2 (+ IMU on one) ──► Visual SLAM ─► TF map→odom→base_link, odometry
               │                                        ▲
               └─ depth + color ──────────────────────┐ │ base_link→cam_i TF (URDF)
                                                       ▼ │
                                                     Nvblox ─► ESDF slice (obstacle map), TSDF mesh
```

Nvblox needs a pose per depth frame, which it reads from the TF that Visual SLAM
publishes. So VSLAM must be tracking before Nvblox output is meaningful.

## TF tree

```text
map --(VSLAM, loop closure)--> odom --(VSLAM, VIO)--> base_link
base_link --(URDF / robot_state_publisher)--> <name>_link --(RealSense driver)--> <name>_*_optical_frame
```

- VSLAM owns `map→odom→base_link`.
- **You** own `base_link → <name>_link` via [description/sev.urdf.xacro](description/sev.urdf.xacro).
- The RealSense driver owns `<name>_link → optical frames`.

Nvblox reconstructs in **`odom`**, not `map`, so loop-closure jumps don't tear
the local obstacle map. That's the right call for local obstacle sensing.

## Files

| Path | Role |
| --- | --- |
| [config/cameras.yaml](config/cameras.yaml) | **Single source of truth** for the SEV camera system: camera names, serials, which one carries the IMU. Every launch file reads it. |
| [description/sev.urdf.xacro](description/sev.urdf.xacro) | Camera mount extrinsics (`base_link → <name>_link`). |
| [launch/bringup.launch.py](launch/bringup.launch.py) | Top-level entry point. Args: `profile` (`dev`\|`jetson`), `use_rviz`. |
| [launch/cameras.launch.py](launch/cameras.launch.py) | One `realsense2_camera` per camera; infra + depth + color, emitter toggling. |
| [launch/visual_slam.launch.py](launch/visual_slam.launch.py) | cuVSLAM over all stereo IR pairs. |
| [launch/nvblox.launch.py](launch/nvblox.launch.py) | Nvblox TSDF/ESDF from all depth streams. |
| [config/nvblox_base.yaml](config/nvblox_base.yaml) | Shared Nvblox params; `nvblox_dev.yaml` / `nvblox_jetson.yaml` layer perf overrides. |

## The IR emitter (why `emitter_on_off: true`)

Same camera, two conflicting needs: VSLAM's infra images want the projector
**off** (dots wreck feature matching); Nvblox's depth wants it **on**. With
`emitter_on_off: true` the D455 toggles the projector every other frame, so depth
uses emitter-on frames and VSLAM mostly sees clean emitter-off frames. Cost:
effective frame rate halves.

> Caveat: cuVSLAM does not itself filter out the occasional emitter-on frame it
> receives. It's usually robust to this. If tracking degrades, the fallbacks are
> a dedicated emitter-off VSLAM camera, or per-frame filtering on emitter metadata.

## Develop on RTX, deploy on Jetson

Isaac ROS runs inside NVIDIA's dev container. The base image is multi-arch, so
**the same container runs on your RTX workstation and on the Jetson (arm64)** —
that's what makes dev/deploy parity work. See [.devcontainer/devcontainer.json](.devcontainer/devcontainer.json)
(a thin wrapper; the canonical path is `isaac_ros_common`'s `run_dev.sh`).

Only **perf dials** differ between platforms, and they live in param files, not
code:

```bash
# RTX workstation
ros2 launch fast_point_cloud bringup.launch.py profile:=dev

# Jetson
ros2 launch fast_point_cloud bringup.launch.py profile:=jetson use_rviz:=false
```

## Seeing RViz from the dev container (X11)

A dev container doesn't forward GUI windows by default. The project's root
`.devcontainer/devcontainer.json` is set up for GUI on a **Linux X11 host**:

- `initializeCommand: xhost +local:root` — authorizes the container to reach the host X server (runs on the host at startup).
- `DISPLAY: ${localEnv:DISPLAY}` + a `/tmp/.X11-unix` bind mount — shares your display and its socket.
- `NVIDIA_DRIVER_CAPABILITIES: all` + `--gpus all` — lets the NVIDIA GPU do the OpenGL rendering RViz needs.

With that, RViz just opens on your desktop:

```bash
ros2 launch fast_point_cloud bringup.launch.py profile:=dev
```

Troubleshooting:

- Window never appears → run `xhost +local:root` on the **host**, then retry.
- Sanity-check the path inside the container: `xeyes` (X works) and
  `glxinfo | grep "OpenGL renderer"` (should name your NVIDIA GPU, not `llvmpipe`).
- Editing `devcontainer.json` requires *Dev Containers: Rebuild Container*, not just reopen.
- `DISPLAY` is captured from the environment VS Code launches in. If it's unset,
  windows fail — `export DISPLAY=:1` before opening, or hardcode it in the config.

This assumes an X11 session (`echo $XDG_SESSION_TYPE` → `x11`). Wayland needs
different handling (XWayland or a Wayland socket mount).

## Dependencies

`isaac_ros_visual_slam`, `isaac_ros_nvblox`, `realsense2_camera`, plus the ROS 2
packages declared in [package.xml](package.xml). Requires an NVIDIA GPU (RTX or
Jetson) — cuVSLAM and Nvblox are CUDA-only.
