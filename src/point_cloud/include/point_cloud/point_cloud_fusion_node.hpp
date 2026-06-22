#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include <std_srvs/srv/empty.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/buffer.hpp>
#include <tf2_ros/transform_listener.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace pointcloud_fusion
{

    class PointCloudFusionNode : public rclcpp::Node
    {
    public:
        explicit PointCloudFusionNode(const rclcpp::NodeOptions &options);

    private:
        void cloudCallback(const sensor_msgs::msg::PointCloud2::ConstSharedPtr &msg, const std::string &topic);
        void clearAccumulatedCloud();
        void fusionTimerCallback();

        std::optional<geometry_msgs::msg::TransformStamped> lookupTransformWithFallback(
            const std::string &source_frame, const rclcpp::Time &stamp, const std::string &topic);

        template <typename PointT>
        pcl::PointCloud<PointT> &accumulatedCloud();

        template <typename PointT>
        void fuseAndPublish(const std::vector<sensor_msgs::msg::PointCloud2> &clouds);

        template <typename PointT>
        void publishCloud(const pcl::PointCloud<PointT> &cloud);

        std::vector<std::string> input_topics_;
        std::string output_topic_;
        std::string target_frame_;
        double publish_rate_hz_{10.0};
        double max_cloud_age_seconds_{1.0};
        bool use_color_{true};
        double voxel_leaf_size_{0.0};
        std::string qos_reliability_{"reliable"};
        bool accumulate_clouds_{false};
        double accumulation_voxel_leaf_size_{0.05};
        size_t max_accumulated_points_{5'000'000};

        std::shared_ptr<tf2_ros::Buffer> tf2_buffer_;
        std::shared_ptr<tf2_ros::TransformListener> tf2_listener_;
        std::vector<rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr> subscriptions_;
        std::map<std::string, sensor_msgs::msg::PointCloud2::ConstSharedPtr> latest_clouds_;

        pcl::PointCloud<pcl::PointXYZ> acc_xyz_;
        pcl::PointCloud<pcl::PointXYZRGB> acc_xyzrgb_;
        std::mutex accumulation_mutex_;
        std::mutex buffer_mutex_;

        rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
        rclcpp::TimerBase::SharedPtr timer_;
        rclcpp::Service<std_srvs::srv::Empty>::SharedPtr clear_service_;
    };

}
