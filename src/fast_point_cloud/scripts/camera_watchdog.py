#!/usr/bin/env python3
# DONT REMOVE THE ABOVE COMMENT OTHERWISE U CAN"T RUN THIS!
"""Health watchdog for the SEV camera pipeline.

Watches every camera's depth source and, for VSLAM cameras, the emitter
state, and names the failing camera and likely cause in the log. Failure
modes it covers (all observed on this rig):

  * Emitter stuck ON despite emitter_on_off: true. The firmware sometimes
    ignores the option at startup and the splitter then starves VSLAM of
    images with no error anywhere. Re-asserting the parameter fixes it.
  * Emitter never ON (emitter_enabled: 0, passive fallback): the splitter
    publishes no depth for that camera.
  * Depth source silent: camera off the USB bus, a USB 2 link that cannot
    start the configured profile, or a dead splitter.
"""

# note this is AI generated - technically this file is not needed but its useful to figure out why the cameras are terrible

import json
import time

import rclpy
from rcl_interfaces.msg import Parameter, ParameterType, ParameterValue
from rcl_interfaces.srv import SetParameters
from rclpy.node import Node
from rclpy.qos import (
    QoSDurabilityPolicy,
    QoSHistoryPolicy,
    QoSProfile,
    QoSReliabilityPolicy,
)
from realsense2_camera_msgs.msg import Metadata
from sensor_msgs.msg import Image

CHECK_PERIOD = 5.0  # seconds between verdicts
WINDOW = 40  # metadata frames per emitter verdict
MIN_MINORITY = 0.25  # healthy flashing is ~50/50 on/off
SETTLE_CYCLES = 2  # verdicts to skip after an emitter intervention
DEPTH_TIMEOUT = 10.0  # seconds of silence before a depth source is "down"
RELOG_PERIOD = 30.0  # repeat an active complaint this often


class CameraWatchdog(Node):
    def __init__(self):
        super().__init__("camera_watchdog")
        p = self.declare_parameter
        self.vslam_cams = [c for c in p("vslam_cameras", [""]).value if c]
        self.depth_only_cams = [c for c in p("depth_only_cameras", [""]).value if c]
        self.grace = p("startup_grace_s", 45.0).value

        qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.VOLATILE,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
        )
        all_cams = self.vslam_cams + self.depth_only_cams
        self.modes = {c: [] for c in self.vslam_cams}
        self.last_meta = {c: None for c in self.vslam_cams}
        self.settle = {c: SETTLE_CYCLES for c in self.vslam_cams}
        self.last_depth = {c: None for c in all_cams}
        self.complaint = {c: None for c in all_cams}  # (text, last_log_time)
        self.param_clients = {}
        self.started = time.time()

        for cam in self.vslam_cams:
            self.create_subscription(
                Metadata,
                f"/{cam}/{cam}/infra1/metadata",
                lambda m, c=cam: self.on_meta(c, m),
                qos,
            )
            self.param_clients[cam] = self.create_client(
                SetParameters, f"/{cam}/{cam}/set_parameters"
            )
        for cam in all_cams:
            # raw=True: arrival times are all we need; skip deserializing
            # 30-60 Hz depth images.
            self.create_subscription(
                Image,
                self.depth_topic(cam),
                lambda m, c=cam: self.on_depth(c),
                qos,
                raw=True,
            )
        self.create_timer(CHECK_PERIOD, self.check)

    def depth_topic(self, cam):
        if cam in self.vslam_cams:
            return f"/{cam}/realsense_splitter_node/output/depth"
        return f"/{cam}/{cam}/depth/image_rect_raw"

    def on_meta(self, cam, msg):
        try:
            mode = json.loads(msg.json_data).get("frame_emitter_mode")
        except (ValueError, AttributeError):
            return
        if mode is None:
            return
        self.last_meta[cam] = time.time()
        buf = self.modes[cam]
        buf.append(int(mode))
        if len(buf) > WINDOW:
            del buf[: len(buf) - WINDOW]

    def on_depth(self, cam):
        self.last_depth[cam] = time.time()

    def emitter_state(self, cam):
        """'alternating' | 'stuck_on' | 'stuck_off' | 'silent' | 'starting'"""
        now = time.time()
        if self.last_meta[cam] is None or now - self.last_meta[cam] > DEPTH_TIMEOUT:
            return "silent"
        buf = self.modes[cam]
        if len(buf) < WINDOW:
            return "starting"
        frac_on = buf.count(1) / len(buf)
        if frac_on > 1.0 - MIN_MINORITY:
            return "stuck_on"
        if frac_on < MIN_MINORITY:
            return "stuck_off"
        return "alternating"

    def check(self):
        if time.time() - self.started < self.grace:
            return
        for cam in self.vslam_cams:
            self.check_vslam_cam(cam)
        for cam in self.depth_only_cams:
            self.check_depth(
                cam,
                "no depth from the driver: camera off the USB bus, or the "
                "link is USB 2 and cannot start the configured profile "
                "(pin stereo_profile to 640x480x15 or move to USB 3)",
            )

    def check_vslam_cam(self, cam):
        state = self.emitter_state(cam)
        if self.settle[cam] > 0:
            self.settle[cam] -= 1
        elif state == "stuck_on":
            self.get_logger().error(
                f"{cam}: emitter stuck ON - the splitter is starving VSLAM"
                " of images. Re-asserting emitter_on_off."
            )
            self.reassert(cam)
            self.modes[cam] = []
            self.settle[cam] = SETTLE_CYCLES
            return  # depth verdict is meaningless mid-intervention

        causes = {
            "silent": "camera not streaming: check USB enumeration and "
            "link speed (needs 5000 Mbps), then replug",
            "stuck_off": "emitter never fires (emitter_enabled: 0 / "
            "passive mode): the splitter publishes no depth. "
            "Set emitter_enabled: 1 + emitter_on_off: true "
            "in realsense_flashing.yaml",
            "stuck_on": "emitter stuck ON (intervention in progress)",
            "alternating": "emitter healthy but the splitter republishes "
            "nothing: is the splitter container running?",
            "starting": "stream starting up",
        }
        self.check_depth(cam, causes[state])

    def check_depth(self, cam, cause):
        now = time.time()
        last = self.last_depth[cam]
        if last is not None and now - last < DEPTH_TIMEOUT:
            if self.complaint[cam] is not None:
                self.get_logger().info(f"{cam}: depth recovered")
                self.complaint[cam] = None
            return
        seen = "never received" if last is None else f"silent {now - last:.0f}s"
        text = f"{cam}: depth {seen} ({self.depth_topic(cam)}) - {cause}"
        prev = self.complaint[cam]
        if prev is None or prev[0] != text or now - prev[1] > RELOG_PERIOD:
            self.get_logger().error(text)
            self.complaint[cam] = (text, now)

    def reassert(self, cam):
        """Set emitter_on_off false, then true ~1 s later; back-to-back
        writes can coalesce in the firmware into a no-op."""
        if not self.param_clients[cam].service_is_ready():
            self.get_logger().warning(f"{cam}: parameter service not ready")
            return
        self.send_emitter(cam, False)
        timer = None

        def fire_true():
            timer.cancel()
            self.send_emitter(cam, True)

        timer = self.create_timer(1.0, fire_true)

    def send_emitter(self, cam, value):
        p = Parameter(
            name="depth_module.emitter_on_off",
            value=ParameterValue(type=ParameterType.PARAMETER_BOOL, bool_value=value),
        )
        self.param_clients[cam].call_async(SetParameters.Request(parameters=[p]))


def main():
    rclpy.init()
    node = CameraWatchdog()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
