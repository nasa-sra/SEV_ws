#!/usr/bin/env python3
# DONT REMOVE THE ABOVE COMMENT OTHERWISE U CAN"T RUN THIS!
import math

import numpy as np
import rclpy
from geometry_msgs.msg import Point
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from rclpy.time import Time
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2
from std_msgs.msg import ColorRGBA, Float32
from tf2_ros import (
    TransformException,
)  # ignore this missing package isaac ros is stupid
from tf2_ros.buffer import Buffer
from tf2_ros.transform_listener import TransformListener
from visualization_msgs.msg import Marker, MarkerArray

# a lot of this is publicly availble github code that was modified, but the basic principle of arcs and a mesh grid is not new. This code is probably the best place to do refactorings on since its a frankenstiened monster

ZONE_COLORS = [  # this is aura
    (0.94, 0.18, 0.16),
    (1.00, 0.55, 0.10),
    (1.00, 0.83, 0.20),
    (0.45, 0.62, 0.68),
]
ZONE_ALPHAS = [0.95, 0.90, 0.80, 0.55]
COMPASS = [
    "FRONT",
    "FRONT-LEFT",
    "LEFT",
    "REAR-LEFT",
    "REAR",
    "REAR-RIGHT",
    "RIGHT",
    "FRONT-RIGHT",
]
MAX_LABELS = 4


def rgba(r, g, b, a):
    return ColorRGBA(r=float(r), g=float(g), b=float(b), a=float(a))


def yaw_from_quaternion(q):
    return math.atan2(
        2.0 * (q.w * q.z + q.x * q.y),
        1.0 - 2.0 * (q.y * q.y + q.z * q.z),
    )


def rounded_rect(center_x, length, width, offset, step_rad=0.324):
    hl, hw = length / 2.0, width / 2.0
    corners = [
        (center_x + hl, +hw, 0.0),
        (center_x - hl, +hw, math.pi / 2.0),
        (center_x - hl, -hw, math.pi),
        (center_x + hl, -hw, 3.0 * math.pi / 2.0),
    ]
    pts = []
    n = max(2, math.ceil((math.pi / 2.0) / step_rad))
    for cx, cy, a0 in corners:
        for k in range(n + 1):
            a = a0 + (math.pi / 2.0) * k / n
            pts.append((cx + offset * math.cos(a), cy + offset * math.sin(a)))
    pts.append(pts[0])
    return pts


class SurroundViewNode(Node):
    def __init__(self):
        super().__init__("surround_view")

        p = self.declare_parameter
        self.esdf_topic = p("esdf_topic", "/nvblox_node/static_esdf_pointcloud").value
        self.vehicle_frame = p("vehicle_frame", "base_link").value
        self.obstacle_threshold = p("obstacle_intensity_threshold", 0.04).value
        self.display_range = p("display_range_m", 8.0).value
        self.grid_res = p("grid_resolution_m", 0.10).value
        self.min_cluster_cells = int(p("min_cluster_cells", 3).value or 0)
        self.column_height = p("column_height_m", 0.45).value
        self.fp_length = p("footprint_length_m", 1.4).value
        self.fp_width = p("footprint_width_m", 1.0).value
        self.fp_center_x = p("footprint_center_x_m", 0.0).value
        self.num_sectors = int(p("num_sectors", 16).value or 16)
        self.arc_max_range = p("arc_max_range_m", 3.5).value
        self.zone_ranges = [
            p("danger_range_m", 0.75).value,
            p("warn_range_m", 1.5).value,
            p("caution_range_m", 3.0).value,
        ]

        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        self.sub = self.create_subscription(
            PointCloud2, self.esdf_topic, self.on_cloud, qos_profile_sensor_data
        )
        self.marker_pub = self.create_publisher(MarkerArray, "~/markers", 1)
        self.nearest_pub = self.create_publisher(
            Float32, "~/nearest_obstacle_distance", 1
        )

        self.last_cloud_time = None
        self.create_timer(1.0, self.check_data_age)

    def vehicle_pose(self, frame_id):
        try:
            tf = self.tf_buffer.lookup_transform(frame_id, self.vehicle_frame, Time())
        except TransformException as ex:
            self.get_logger().warning(
                f"no transform {frame_id}->{self.vehicle_frame}: {ex}",
                throttle_duration_sec=5.0,
            )
            return None
        t = tf.transform.translation
        return t.x, t.y, yaw_from_quaternion(tf.transform.rotation)

    def on_cloud(self, msg):
        self.last_cloud_time = self.get_clock().now()
        pose = self.vehicle_pose(msg.header.frame_id)
        if pose is None:
            self.publish_state(msg, None, "NO POSE")
            return
        points = point_cloud2.read_points(msg, field_names=["x", "y", "z", "intensity"])
        self.publish_state(msg, self.extract_obstacle_cells(points, pose), None)

    def extract_obstacle_cells(self, arr, pose):
        tx, ty, tyaw = pose
        x, y = arr["x"], arr["y"]
        mask = arr["intensity"] < self.obstacle_threshold
        mask &= (np.abs(x - tx) < self.display_range) & (
            np.abs(y - ty) < self.display_range
        )
        if not mask.any():
            return None
        x = x[mask].astype(np.float64)
        y = y[mask].astype(np.float64)
        z_ground = float(arr["z"][mask][0])

        ij = np.stack(
            [
                np.floor(x / self.grid_res).astype(np.int64),
                np.floor(y / self.grid_res).astype(np.int64),
            ],
            axis=1,
        )
        ij = np.unique(ij, axis=0)
        ij = ij[self.speckle_filter(ij)]
        if len(ij) == 0:
            return None
        cx = (ij[:, 0] + 0.5) * self.grid_res
        cy = (ij[:, 1] + 0.5) * self.grid_res

        # copied math
        dx, dy = cx - tx, cy - ty
        c, s = math.cos(-tyaw), math.sin(-tyaw)
        vx = c * dx - s * dy
        vy = s * dx + c * dy
        qx = np.abs(vx - self.fp_center_x) - self.fp_length / 2.0
        qy = np.abs(vy) - self.fp_width / 2.0
        d_fp = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0))
        radial = np.hypot(vx, vy)
        angle = np.arctan2(vy, vx)

        keep = (radial < self.display_range) & (d_fp > 1e-6)
        if not keep.any():
            return None
        return {
            "odom_xy": (cx[keep], cy[keep]),
            "z_ground": z_ground,
            "d_fp": d_fp[keep],
            "radial": radial[keep],
            "angle": angle[keep],
        }

    # this was copied
    def speckle_filter(self, ij):
        n = len(ij)
        if self.min_cluster_cells <= 1:
            return np.ones(n, dtype=bool)
        index = {(int(i), int(j)): k for k, (i, j) in enumerate(ij)}
        keep = np.zeros(n, dtype=bool)
        seen = set()
        for start in index:
            if start in seen:
                continue
            seen.add(start)
            cluster = [start]
            queue = [start]
            while queue:
                ci, cj = queue.pop()
                for di in (-1, 0, 1):
                    for dj in (-1, 0, 1):
                        nb = (ci + di, cj + dj)
                        if nb not in seen and nb in index:
                            seen.add(nb)
                            cluster.append(nb)
                            queue.append(nb)
            if len(cluster) >= self.min_cluster_cells:
                for cell in cluster:
                    keep[index[cell]] = True
        return keep

    def zone_of(self, d):
        for zone, r in enumerate(self.zone_ranges):
            if d < r:
                return zone
        return len(self.zone_ranges)

    def base_marker(self, ns, mid, mtype, frame, stamp, locked=True):
        m = Marker()
        m.header.frame_id = frame
        m.header.stamp = stamp
        m.ns = ns
        m.id = mid
        m.type = mtype
        m.action = Marker.ADD
        m.pose.orientation.w = 1.0
        m.frame_locked = locked
        return m

    def status_marker(self, stamp, text, color):
        status = self.base_marker(
            "status", 0, Marker.TEXT_VIEW_FACING, self.vehicle_frame, stamp
        )
        status.pose.position.z = 1.5
        status.scale.z = 0.35
        status.text = text
        status.color = color
        return status

    def publish_state(self, msg, cells, error_text):
        stamp = msg.header.stamp
        out = MarkerArray()
        out.markers.append(Marker(action=Marker.DELETEALL))
        self.add_vehicle_markers(out, stamp)

        nearest = None
        if cells is not None and len(cells["d_fp"]) > 0:
            self.add_columns(out, cells, msg.header.frame_id, stamp)
            nearest = self.add_sectors(out, cells, stamp)

        self.add_status(out, stamp, nearest, error_text)
        self.marker_pub.publish(out)
        self.nearest_pub.publish(Float32(data=float(nearest[0]) if nearest else -1.0))

    def add_vehicle_markers(self, out, stamp):
        fp = self.base_marker(
            "footprint", 0, Marker.LINE_STRIP, self.vehicle_frame, stamp
        )
        fp.scale.x = 0.03
        fp.color = rgba(0.9, 0.95, 1.0, 0.5)
        hl, hw = self.fp_length / 2.0, self.fp_width / 2.0
        for px, py in [(hl, hw), (-hl, hw), (-hl, -hw), (hl, -hw), (hl, hw)]:
            fp.points.append(Point(x=px + self.fp_center_x, y=py, z=0.03))
        out.markers.append(fp)

        for zone, zone_range in enumerate(self.zone_ranges):
            ring = self.base_marker(
                "zone_rings",
                zone,
                Marker.LINE_STRIP,
                self.vehicle_frame,
                stamp,
            )
            ring.scale.x = 0.02
            ring.color = rgba(*ZONE_COLORS[zone], 0.22)
            for px, py in rounded_rect(
                self.fp_center_x, self.fp_length, self.fp_width, zone_range
            ):
                ring.points.append(Point(x=px, y=py, z=0.02))
            out.markers.append(ring)

    def add_columns(self, out, cells, frame, stamp):
        cx, cy = cells["odom_xy"]
        m = self.base_marker("columns", 0, Marker.CUBE_LIST, frame, stamp, locked=False)
        m.scale.x = m.scale.y = 0.9 * self.grid_res
        m.scale.z = self.column_height
        zc = cells["z_ground"] + self.column_height / 2.0
        for k in range(len(cx)):
            m.points.append(Point(x=float(cx[k]), y=float(cy[k]), z=zc))
            zone = self.zone_of(cells["d_fp"][k])
            m.colors.append(rgba(*ZONE_COLORS[zone], ZONE_ALPHAS[zone]))
        out.markers.append(m)

    def add_sectors(self, out, cells, stamp):
        n = self.num_sectors
        sec_width = 2.0 * math.pi / n
        sec = (
            np.floor((cells["angle"] + sec_width / 2.0) / sec_width).astype(np.int64)
            % n
        )

        order = np.argsort(cells["d_fp"], kind="stable")
        uniq, first = np.unique(sec[order], return_index=True)
        rep = order[first]

        arcs = self.base_marker(
            "arcs", 0, Marker.TRIANGLE_LIST, self.vehicle_frame, stamp
        )
        arcs.scale.x = arcs.scale.y = arcs.scale.z = 1.0

        nearest = None
        label_candidates = []
        for s, k in zip(uniq, rep):
            d = float(cells["d_fp"][k])
            if nearest is None or d < nearest[0]:
                nearest = (d, float(cells["angle"][k]))
            if d >= self.arc_max_range:
                continue
            r = max(0.3, float(cells["radial"][k]))
            color = rgba(*ZONE_COLORS[self.zone_of(d)], 0.85)
            a_mid = s * sec_width
            self.append_arc(arcs, r, a_mid, sec_width, color)
            if d < self.zone_ranges[1]:
                label_candidates.append((d, int(s), r, a_mid))

        self.add_labels(out, label_candidates, stamp)
        if arcs.points:
            out.markers.append(arcs)
        return nearest

    def append_arc(self, arcs, r, a_mid, sec_width, color, steps=6):
        a0 = a_mid - sec_width / 2.0 + 0.03
        a1 = a_mid + sec_width / 2.0 - 0.03
        for t in range(steps):
            b0 = a0 + (a1 - a0) * t / steps
            b1 = a0 + (a1 - a0) * (t + 1) / steps
            quad = [
                (r * math.cos(b0), r * math.sin(b0)),
                ((r + 0.09) * math.cos(b0), (r + 0.09) * math.sin(b0)),
                (r * math.cos(b1), r * math.sin(b1)),
                ((r + 0.09) * math.cos(b1), (r + 0.09) * math.sin(b1)),
            ]
            for idx in (0, 1, 2, 2, 1, 3):
                px, py = quad[idx]
                arcs.points.append(Point(x=px, y=py, z=0.05))
                arcs.colors.append(color)

    def add_labels(self, out, candidates, stamp):
        n = self.num_sectors
        candidates.sort()
        labeled = []
        for d, s, r, a_mid in candidates:
            if len(labeled) >= MAX_LABELS:
                break
            if any(min((s - t) % n, (t - s) % n) <= 1 for t in labeled):
                continue
            labeled.append(s)
            label = self.base_marker(
                "sector_labels",
                s,
                Marker.TEXT_VIEW_FACING,
                self.vehicle_frame,
                stamp,
            )
            label.text = f"{d:.1f}"
            label.scale.z = 0.22
            label.color = rgba(1.0, 1.0, 1.0, 0.95)
            label.pose.position.x = (r + 0.45) * math.cos(a_mid)
            label.pose.position.y = (r + 0.45) * math.sin(a_mid)
            label.pose.position.z = 0.3
            out.markers.append(label)

    def add_status(self, out, stamp, nearest, error_text):
        if error_text:
            marker = self.status_marker(stamp, error_text, rgba(1.0, 0.3, 0.3, 0.9))
        elif nearest and nearest[0] < self.zone_ranges[2]:
            d, ang = nearest
            octant = int(round(ang / (math.pi / 4.0))) % 8
            color = rgba(*ZONE_COLORS[self.zone_of(d)], 1.0)
            marker = self.status_marker(stamp, f"{COMPASS[octant]}  {d:.1f} m", color)
        else:
            marker = self.status_marker(stamp, "CLEAR", rgba(0.5, 0.9, 0.6, 0.7))
        out.markers.append(marker)

    def check_data_age(self):
        if self.last_cloud_time is None:
            return
        age = (self.get_clock().now() - self.last_cloud_time).nanoseconds
        if age > 3e9:
            out = MarkerArray()
            out.markers.append(Marker(action=Marker.DELETEALL))
            out.markers.append(
                self.status_marker(
                    self.get_clock().now().to_msg(),
                    "NO MAP DATA",
                    rgba(1.0, 0.3, 0.3, 0.9),
                )
            )
            self.marker_pub.publish(out)
            self.nearest_pub.publish(Float32(data=-1.0))


def main():
    rclpy.init()
    node = SurroundViewNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
