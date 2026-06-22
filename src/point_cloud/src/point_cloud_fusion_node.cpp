#include "point_cloud/point_cloud_fusion_node.hpp"

#include <algorithm>
#include <chrono>

#include <tf2/exceptions.hpp>
#include <tf2/time.hpp>
#include <tf2_sensor_msgs/tf2_sensor_msgs.hpp>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_conversions/pcl_conversions.h>

namespace pointcloud_fusion
{

    namespace
    {
        constexpr double kTransformLookupTimeoutSec = 0.15;
        constexpr int64_t kWarnThrottleMs = 5000;
    }

    PointCloudFusionNode::PointCloudFusionNode(const rclcpp::NodeOptions &options)
        : Node("pointcloud_fusion_node", options)
    {
        input_topics_ = declare_parameter<std::vector<std::string>>("input_topics", std::vector<std::string>{});
        output_topic_ = declare_parameter<std::string>("output_topic", "/fused_point_cloud");
        target_frame_ = declare_parameter<std::string>("target_frame", "world");
        publish_rate_hz_ = declare_parameter<double>("publish_rate_hz", 10.0);
        max_cloud_age_seconds_ = declare_parameter<double>("max_cloud_age_seconds", 1.0);
        use_color_ = declare_parameter<bool>("use_color", true);
        voxel_leaf_size_ = declare_parameter<double>("voxel_leaf_size", 0.0);
        qos_reliability_ = declare_parameter<std::string>("qos_reliability", "reliable");
        accumulate_clouds_ = declare_parameter<bool>("accumulate_clouds", false);
        accumulation_voxel_leaf_size_ = declare_parameter<double>("accumulation_voxel_leaf_size", 0.05);
        max_accumulated_points_ = static_cast<size_t>(declare_parameter<int>("max_accumulated_points", 5'000'000));

        if (input_topics_.empty())
        {
            RCLCPP_ERROR(get_logger(), "No 'input_topics' provided for this node to subscribe to");
        }

        tf2_buffer_ = std::make_shared<tf2_ros::Buffer>(get_clock());
        tf2_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf2_buffer_);

        rclcpp::QoS qos(rclcpp::KeepLast(5));
        qos_reliability_ == "best_effort" ? qos.best_effort() : qos.reliable();

        subscriptions_.reserve(input_topics_.size());
        for (const auto &topic : input_topics_)
        {
            auto sub = create_subscription<sensor_msgs::msg::PointCloud2>(
                topic, qos,
                [this, topic](const sensor_msgs::msg::PointCloud2::ConstSharedPtr msg)
                {
                    cloudCallback(msg, topic);
                });
            subscriptions_.push_back(sub);
            RCLCPP_INFO(get_logger(), "Subscribed to input topic '%s'", topic.c_str());
        }

        publisher_ = create_publisher<sensor_msgs::msg::PointCloud2>(output_topic_, rclcpp::QoS(5));

        const double rate = std::max(publish_rate_hz_, 0.1);
        const auto period = std::chrono::duration<double>(1.0 / rate);
        timer_ = create_wall_timer(
            std::chrono::duration_cast<std::chrono::milliseconds>(period),
            std::bind(&PointCloudFusionNode::fusionTimerCallback, this));

        clear_service_ = create_service<std_srvs::srv::Empty>(
            "~/clear_accumulated_cloud",
            [this](const std_srvs::srv::Empty::Request::SharedPtr, std_srvs::srv::Empty::Response::SharedPtr)
            {
                clearAccumulatedCloud();
            });

        RCLCPP_INFO(
            get_logger(),
            "pointcloud_fusion_node started: %zu topic(s) -> '%s' in frame '%s' @ %.1f Hz | "
            "use_color=%s | voxel=%.3f | accumulate=%s (acc_voxel=%.3f, max_pts=%zu)",
            input_topics_.size(), output_topic_.c_str(), target_frame_.c_str(), publish_rate_hz_,
            use_color_ ? "true" : "false", voxel_leaf_size_, accumulate_clouds_ ? "true" : "false",
            accumulation_voxel_leaf_size_, max_accumulated_points_);
    }

    void PointCloudFusionNode::cloudCallback(
        const sensor_msgs::msg::PointCloud2::ConstSharedPtr &msg, const std::string &topic)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        latest_clouds_[topic] = msg;
    }

    std::optional<geometry_msgs::msg::TransformStamped> PointCloudFusionNode::lookupTransformWithFallback(
        const std::string &source_frame, const rclcpp::Time &stamp, const std::string &topic)
    {
        try
        {
            return tf2_buffer_->lookupTransform(
                target_frame_, source_frame, stamp, rclcpp::Duration::from_seconds(kTransformLookupTimeoutSec));
        }
        catch (const tf2::TransformException &ex)
        {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), kWarnThrottleMs,
                "Could not look up transform from '%s' to '%s' for topic '%s': %s. Trying latest available transform.",
                source_frame.c_str(), target_frame_.c_str(), topic.c_str(), ex.what());
        }

        try
        {
            return tf2_buffer_->lookupTransform(target_frame_, source_frame, tf2::TimePointZero);
        }
        catch (const tf2::TransformException &ex)
        {
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), kWarnThrottleMs,
                "Fallback TF lookup also failed for topic '%s': %s", topic.c_str(), ex.what());
            return std::nullopt;
        }
    }

    void PointCloudFusionNode::fusionTimerCallback()
    {
        std::map<std::string, sensor_msgs::msg::PointCloud2::ConstSharedPtr> snapshot;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            snapshot = latest_clouds_;
        }

        if (snapshot.empty())
        {
            return;
        }

        std::vector<sensor_msgs::msg::PointCloud2> transformed_clouds;
        transformed_clouds.reserve(snapshot.size());

        const rclcpp::Time now = this->now();

        for (const auto &[topic, msg] : snapshot)
        {
            if (!msg)
            {
                continue;
            }
            const rclcpp::Time stamp(msg->header.stamp, now.get_clock_type());
            const double age = (now - stamp).seconds();
            if (age > max_cloud_age_seconds_)
            {
                RCLCPP_WARN_THROTTLE(
                    get_logger(), *get_clock(), kWarnThrottleMs,
                    "Dropping stale cloud from '%s' (age %.2fs > max_cloud_age_sec %.2fs)",
                    topic.c_str(), age, max_cloud_age_seconds_);
                continue;
            }

            const auto transform = lookupTransformWithFallback(msg->header.frame_id, stamp, topic);
            if (!transform)
            {
                continue;
            }

            sensor_msgs::msg::PointCloud2 transformed;
            tf2::doTransform(*msg, transformed, *transform);
            transformed_clouds.push_back(std::move(transformed));
        }

        if (transformed_clouds.empty())
        {
            return;
        }

        if (use_color_)
        {
            fuseAndPublish<pcl::PointXYZRGB>(transformed_clouds);
        }
        else
        {
            fuseAndPublish<pcl::PointXYZ>(transformed_clouds);
        }
    }

    template <typename PointT>
    void PointCloudFusionNode::fuseAndPublish(const std::vector<sensor_msgs::msg::PointCloud2> &clouds)
    {
        pcl::PointCloud<PointT> fused_cloud;
        for (const auto &cloud_msg : clouds)
        {
            pcl::PointCloud<PointT> cloud;
            pcl::fromROSMsg(cloud_msg, cloud);
            fused_cloud += cloud;
        }

        if (fused_cloud.empty())
        {
            return;
        }

        if (voxel_leaf_size_ > 0.0)
        {
            typename pcl::PointCloud<PointT>::Ptr fused_ptr(new pcl::PointCloud<PointT>(fused_cloud));
            pcl::VoxelGrid<PointT> voxel_filter;
            voxel_filter.setInputCloud(fused_ptr);
            const float leaf = static_cast<float>(voxel_leaf_size_);
            voxel_filter.setLeafSize(leaf, leaf, leaf);
            pcl::PointCloud<PointT> filtered_cloud;
            voxel_filter.filter(filtered_cloud);
            fused_cloud.swap(filtered_cloud);
        }

        if (!accumulate_clouds_)
        {
            publishCloud(fused_cloud);
            return;
        }

        std::lock_guard<std::mutex> lock(accumulation_mutex_);
        auto &acc = accumulatedCloud<PointT>();
        acc += fused_cloud;

        if (acc.size() > max_accumulated_points_)
        {
            typename pcl::PointCloud<PointT>::Ptr ptr(new pcl::PointCloud<PointT>(acc));
            pcl::VoxelGrid<PointT> vg;
            vg.setInputCloud(ptr);
            const float leaf = static_cast<float>(accumulation_voxel_leaf_size_);
            vg.setLeafSize(leaf, leaf, leaf);
            pcl::PointCloud<PointT> compacted;
            vg.filter(compacted);
            acc.swap(compacted);
            RCLCPP_DEBUG(get_logger(), "Accumulated cloud compacted: %zu -> %zu points", ptr->size(), acc.size());
        }

        publishCloud(acc);
    }

    template <typename PointT>
    void PointCloudFusionNode::publishCloud(const pcl::PointCloud<PointT> &cloud)
    {
        sensor_msgs::msg::PointCloud2 output;
        pcl::toROSMsg(cloud, output);
        output.header.frame_id = target_frame_;
        output.header.stamp = now();
        publisher_->publish(output);
    }

    template <>
    pcl::PointCloud<pcl::PointXYZ> &PointCloudFusionNode::accumulatedCloud<pcl::PointXYZ>()
    {
        return acc_xyz_;
    }

    template <>
    pcl::PointCloud<pcl::PointXYZRGB> &PointCloudFusionNode::accumulatedCloud<pcl::PointXYZRGB>()
    {
        return acc_xyzrgb_;
    }

    void PointCloudFusionNode::clearAccumulatedCloud()
    {
        std::lock_guard<std::mutex> lock(accumulation_mutex_);
        acc_xyz_.clear();
        acc_xyzrgb_.clear();
        RCLCPP_INFO(get_logger(), "Accumulated cloud cleared.");
    }

}
