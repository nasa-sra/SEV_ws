import rclpy
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix
from std_srvs.srv import Trigger
import yaml
import math
import os
from datetime import datetime
import typing

class PathRecorderNode(Node):
    def __init__(self):
        super().__init__("path_recorder")
        self.declare_parameter("min_distance_meters", 0.5)
        self.declare_parameter("output_dir", os.path.expanduser('~/paths'))
        self.min_dist = self.get_parameter("min_distance_meters").value
        self.output_dir: str = self.get_parameter("output_dir").value # type: ignore
        os.makedirs(self.output_dir, exist_ok=True)
        
        
        self.waypoints = []
        self.last_fix = None
        self.recording = False