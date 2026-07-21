# fast_point_cloud

<!-- This documentation is mostly AI generated -->
GPU-accelerated perception for the Lunar SEV. Multiple Intel RealSense
D455 cameras feed two Isaac ROS components:

- **Isaac ROS Visual SLAM (cuVSLAM)** → **pose** (TF `map → odom → base_link`, `/visual_slam/tracking/odometry`)
- **Isaac ROS Nvblox** → **obstacle map** (a 2D ESDF distance slice + 3D TSDF mesh)

On top of that sits the **driver display**: a Tesla-style 360° surround view
(`surround_view_node.py`) that turns the ESDF slice into proximity-colored
obstacle columns, parking-sensor arcs and distance readouts around the SEV.

This package **does not do path planning or avoidance** — a downstream
consumer reads the ESDF slice / TF for that.

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
| [scripts/surround_view_node.py](scripts/surround_view_node.py) | ESDF slice → driver display markers (columns, arcs, readouts). |
| [config/surround_view.yaml](config/surround_view.yaml) | Surround-view tuning: footprint size, zone distances, sector count. |
| [config/perception.rviz](config/perception.rviz) | Driver RViz layout: chase cam + mesh + surround overlay. |

## The driver display (Tesla-style surround view)

`surround_view_node.py` consumes `/nvblox_node/static_esdf_pointcloud` (the
2D ESDF slice: per-cell distance-to-obstacle) and publishes
`/surround_view/markers`, an ego-centric overlay designed to be read at a
glance:

- **Obstacle columns** — solid boxes over every obstacle cell, colored by
  distance from the SEV **footprint edge**: red < 0.75 m, orange < 1.5 m,
  amber < 3 m, calm gray-blue beyond. Isolated one-cell blips are hidden
  (`min_cluster_cells`), so sensor speckle and the last crumbs of a decaying
  ghost never reach the driver.
- **Parking-sensor arcs** — 16 sectors around the SEV; each sector with an
  obstacle inside 3.5 m gets an arc at that obstacle's range (plus a distance
  label when < 1.5 m).
- **Zone halos** — faint red/orange/amber contours hugging the footprint, so
  the colors explain themselves.
- **Status readout** — floating text above the SEV: `FRONT-LEFT 0.8 m`,
  `CLEAR`, or `NO MAP DATA` when the pipeline goes quiet (watchdog).
- `/surround_view/nearest_obstacle_distance` (`Float32`, −1 = clear) for any
  downstream alerting (audio, dashboard).

Tune footprint size, zone distances and sector count in
[config/surround_view.yaml](config/surround_view.yaml). The RViz layout
([config/perception.rviz](config/perception.rviz)) opens in a **chase camera**
locked behind the SEV (`ThirdPersonFollower`), with the nvblox color mesh as
the "high-fidelity" world context; switch to the **Overhead** view in the
Views panel for a top-down parking view. The old black-and-white occupancy
grid is still available as a disabled diagnostic display.

Foxglove note: `visualization_msgs/MarkerArray` renders fine in Foxglove, so
the surround view works there too — the nvblox mesh does not (custom msg).

## Ghost obstacles (map decay)

A pure static-TSDF map never forgets: an obstacle that was moved kept
haunting the map if no camera re-observed (or could re-observe) that spot.
`nvblox_base.yaml` now enables:

- **TSDF decay** — every voxel's confidence decays at 5 Hz
  (`tsdf_decay_factor: 0.92`); re-observed voxels are constantly re-boosted,
  unobserved ones drop out of the obstacle map after ~9 s and are deallocated
  (mesh chunk deleted) after ~20 s; low-weight motion-smear ghosts fade in
  ~5 s. Lower the factor for faster forgetting — but remember the SEV has
  **no rear camera**: whatever it just drove past survives only in map
  memory, so don't forget too fast.
- **Invalid-depth decay** (`projective_tsdf_integrator_invalid_depth_decay_factor`)
  — erases ghosts even *in view* when the revealed background returns no
  depth (out of range / dark), which normal free-space carving can't fix.
- **Radius clearing** (`map_clearing_radius_m: 9.0`) — anything farther than
  9 m from `base_link` is dropped once a second; the display only shows 8 m.
- **Weight cap** (`projective_integrator_max_weight: 5.0`) — a long-observed
  obstacle can't bank so much confidence that carving takes seconds to erase
  it once it moves.

## Doubled/tripled walls while moving (pose vs map hygiene)

If walls duplicate at slightly rotated offsets **while the rig moves** and
only slowly merge back, that is not decay's job — each depth frame is being
integrated at a subtly wrong pose. Four dials control this, in order of
impact:

1. **One VSLAM camera, not two.** Two free-running D455s (no HW sync cable)
   feeding cuVSLAM Multicamera mode produce framesets mixing images up to
   ~17 ms apart; the resulting pose jitter paints a fresh wall layer per
   view. `cameras.yaml` therefore runs **single-camera VIO** (the config
   NVIDIA ships) and uses D455_2 as a depth-only nvblox camera — which also
   means its emitter stays solid ON: full-rate, better depth. If you add a
   HW sync cable (`inter_cam_sync_mode` master/slave), multicam VSLAM
   becomes viable again.
2. **VSLAM cameras must run 60 fps stereo** (`stereo_profile: 848x480x60`).
   Emitter flashing halves the rate; at the old 30 fps default, cuVSLAM
   tracked at 15 Hz and motion between frames was large enough to smear.
3. **Voxel size is cubic in cost** (`nvblox_dev.yaml`): 0.02 m costs ~15.6×
   NVIDIA's 0.05 reference and the integrator falls behind exactly when the
   view changes. Dev profile runs 0.04; go to 0.05 before touching anything
   else if smearing persists.
4. **TSDF-distance-penalty weighting**
   (`projective_integrator_weighting_mode`) — NVIDIA's reference mode;
   down-weights measurements that land far behind the current surface, so a
   brief pose error thickens the wall less and carves out faster.

Two guard nodes now run automatically because of failures observed live:

- **`camera_watchdog.py`** (started by `cameras.launch.py`): watches every
  camera's depth source and the VSLAM cameras' emitter state, and names
  the failing camera and likely cause in the log. Silent failure modes it
  catches: emitter stuck ON despite `emitter_on_off: true` (firmware
  ignored the startup setting → splitter starves VSLAM; the watchdog
  re-asserts the parameter, which fixes it), emitter never firing
  (`emitter_enabled: 0` → the splitter publishes **no depth** for that
  camera), and a silent depth stream (camera off the USB bus, USB 2 link
  that can't start the profile, or a dead splitter).
- **`wait_for_stable_pose.py`** (gates nvblox in `bringup.launch.py`):
  cuVSLAM's first ~30–60 s can contain pose teleports even with a static
  camera (measured: 15 cm / 27° jumps). nvblox only starts once
  `odom→base_link` has been jump-free for `stable_seconds`. NOTE: its
  `timeout_seconds` must be longer than camera + VSLAM startup (cameras
  alone stagger 10 s each) or the gate times out and releases nvblox before
  a pose even exists.

### Bench-environment gotchas (measured on the lab rig)

- **Dim IR at night**: VSLAM uses *emitter-OFF* frames = passive IR. In a
  dark room they run ~33/255 mean brightness — weak features, fragile
  tracking, worse under motion blur. Bench-test with the room lit; this is
  a non-issue outdoors.
- **USB link health is everything.** Symptoms seen live: `control_transfer
  returned error` / `set_xu failed` spam (flaky control channel — exposure
  and emitter commands silently fail), cuVSLAM warning `Delta between
  current and previous frame [66.8 ms]` (dropped frames → tracking gaps),
  cameras enumerating at 480 Mbps (USB 2 — high-fps profiles won't start),
  and cameras vanishing from the bus entirely after many restart cycles
  (only a physical replug recovers). Check with:
  `for d in /sys/bus/usb/devices/*/; do echo "$d $(cat $d/product 2>/dev/null) $(cat $d/speed 2>/dev/null)"; done | grep RealSense`
  — every camera must show **5000** (USB 3) and sit on its own controller.

## Camera roles (`vslam` flag in cameras.yaml)

Not every camera should feed cuVSLAM. Its **Multicamera mode requires
time-aligned framesets from every configured stereo pair**, and our cameras are
not hardware-synced — each un-synced camera added to VSLAM makes tracking less
likely to ever start. So each camera declares a role:

- `vslam: true` — stereo IR pair feeds cuVSLAM. Emitter *flashes*; a splitter
  routes emitter-off infra to VSLAM and emitter-on depth to nvblox.
- `vslam: false` — depth-only coverage camera (currently **D455_2**, and the
  **D435** when enabled). Emitter is *solid on* (best active-stereo depth),
  infra streams are disabled entirely (saves USB bandwidth), no splitter
  runs, and nvblox reads the driver's depth topic directly.

This mirrors NVIDIA's multi-RealSense example (VSLAM on the primary camera,
extra cameras as depth sources). nvblox supports at most **4 cameras** total.

## The IR emitter (flashing + splitter)

Same camera, two conflicting needs: VSLAM's infra images want the projector
**off** (the dot pattern wrecks feature matching); Nvblox's depth wants it
**on** (active stereo). We satisfy both with NVIDIA's design:

1. Cameras run `depth_module.emitter_on_off: true` — the projector toggles every
   other frame (at 60 fps capture → ~30 Hz per consumer).
2. A **`realsense_splitter`** node per camera (vendored NVIDIA source at
   `src/realsense_splitter`, built in this workspace) reads each frame's emitter
   state from the RealSense **metadata** topics and routes:
   - `~/output/infra_1`, `~/output/infra_2` (emitter-**off**, clean) → Visual SLAM
   - `~/output/depth` (emitter-**on**, active stereo) → Nvblox

Consequences of this wiring:

- **The emitter params live in [config/realsense_flashing.yaml](config/realsense_flashing.yaml),
  passed to `rs_launch.py` via its `config_file` argument — NOT as launch
  arguments.** `rs_launch.py` silently drops any argument it doesn't declare,
  and `depth_module.emitter_*` are not declared. (Found the hard way: passed as
  launch args, `emitter_on_off` stayed `False` and VSLAM starved.)
- **Cameras start staggered 10 s apart** (`TimerAction` in `cameras.launch.py`).
  Launching multiple RealSense nodes simultaneously hits a driver race where one
  camera silently never streams — no error, node alive, zero data. NVIDIA's
  multi-camera example uses the same delay workaround.
- Nvblox consumes the splitter's **raw depth** (depth optical frame + its
  `depth/camera_info`); `align_depth` is disabled. Nvblox handles depth/color
  living in different frames via TF.
- Depth/infra profiles must be a real 60 fps D455 mode (`848x480x60`); 1280x720
  only does 30 fps, which would leave each path at 15 Hz.
- If the splitter publishes nothing, check that the camera's `.../metadata`
  topics exist and tick — frame metadata is how it tells frames apart. If
  metadata is missing on your kernel/librealsense combo, that's a udev/driver
  issue to fix first.

> Fallback: if flashing ever misbehaves, emitter OFF always
> (`depth_module.emitter_enabled: 0`, no splitter, VSLAM/nvblox remaps back to
> the raw driver topics) is the simple degraded mode — passive-stereo depth,
> fine on textured terrain.

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
