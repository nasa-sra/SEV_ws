#!/usr/bin/env python3
# DONT REMOVE THE ABOVE COMMENT OTHERWISE U CAN"T RUN THIS!
import math
import time

import rclpy
from rclpy.node import Node
from tf2_msgs.msg import TFMessage


# copied
def yaw_of(q):
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    )


# an important note about waiting for the poses is that without a stable pose, isaac vslam becomes extremely vulnerable to runaway error, DO NOT MOVE THE SEV WITHOUT A STABLE POSE!
class PoseGate(Node):
    def __init__(self):
        super().__init__("wait_for_stable_pose")
        p = self.declare_parameter
        self.parent = p("parent_frame", "odom").value
        self.child = p("child_frame", "base_link").value
        self.stable_seconds = float(p("stable_seconds", 8.0).value or 8.0)
        self.max_step_m = float(p("max_step_m", 0.06).value or 0.06)
        self.max_yaw_step_deg = float(p("max_yaw_step_deg", 2.0).value or 2.0)
        self.timeout = float(p("timeout_seconds", 10.0).value or 10.0)

        self.prev = None
        self.stable_since = None
        self.started = time.time()
        self.done = False
        self.create_subscription(TFMessage, "/tf", self.on_tf, 100)
        self.get_logger().info(
            f"waiting for {self.parent}->{self.child} to hold steady "
            f"for {self.stable_seconds:.0f}s"
        )

    def on_tf(self, msg):
        now = time.time()
        for t in msg.transforms:
            if t.header.frame_id != self.parent or t.child_frame_id != self.child:
                continue
            tr, q = t.transform.translation, t.transform.rotation
            cur = (tr.x, tr.y, tr.z, yaw_of(q))
            if self.prev is not None:
                step = math.dist(self.prev[:3], cur[:3])
                dyaw = math.degrees(
                    abs(math.remainder(cur[3] - self.prev[3], 2.0 * math.pi))
                )
                if step > self.max_step_m or dyaw > self.max_yaw_step_deg:
                    self.stable_since = None
                elif self.stable_since is None:
                    self.stable_since = now
            self.prev = cur
            if (
                self.stable_since is not None
                and now - self.stable_since >= self.stable_seconds
            ):
                self.get_logger().info("pose stable, starting nvblox")
                self.done = True

    def expired(self):
        return time.time() - self.started > self.timeout


def main():
    rclpy.init()
    gate = PoseGate()
    while rclpy.ok() and not gate.done and not gate.expired():
        rclpy.spin_once(gate, timeout_sec=0.2)
    if not gate.done:
        gate.get_logger().warning(
            f"pose not stable after {gate.timeout:.0f}s, starting nvblox anyway"
        )
    gate.destroy_node()
    rclpy.try_shutdown()


if __name__ == "__main__":
    main()
