#include "point_cloud/point_cloud_fusion_node.hpp"

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::NodeOptions options;
    auto node = std::make_shared<pointcloud_fusion::PointCloudFusionNode>(options);
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}