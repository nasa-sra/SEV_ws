#pragma once


#include <atomic>
#include <mutex>
#include <thread>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include <nav_msgs/msg/odometry.hpp>

#include <sensor_msgs/msg/imu.hpp>

#include <sensor_msgs/msg/nav_sat_fix.hpp>

#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>


#include <nlohmann/json.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

class WebsocketBridge : public rclcpp::Node
{
public:
    WebsocketBridge();
    ~WebsocketBridge();

private:

    //---------------------------------
    // ROS
    //---------------------------------

    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gnss_sub_;


    

    rclcpp::TimerBase::SharedPtr timer_;

    //---------------------------------
    // Latest data
    //---------------------------------

    nav_msgs::msg::Odometry odom_;
    
    sensor_msgs::msg::Imu imu_;

    geometry_msgs::msg::PoseWithCovarianceStamped pose_;

    std::mutex data_mutex_;

    sensor_msgs::msg::NavSatFix gnss_;

    bool odom_received_;
    bool pose_received_;
    bool imu_received_;
    bool gnss_received_;

    //---------------------------------
    // Parameters
    //---------------------------------

    std::string websocket_uri_;
    double publish_rate_;
    
    bool loadConfig(const std::string& config_path);

    //---------------------------------
    // WebSocket
    //---------------------------------

    using Client =
        websocketpp::client<websocketpp::config::asio_client>;

    Client client_;

    websocketpp::connection_hdl connection_;

    std::thread websocket_thread_;

    std::atomic<bool> connected_;

    //---------------------------------
    // Callbacks
    //---------------------------------

    void odomCallback(
        const nav_msgs::msg::Odometry::SharedPtr msg);

    void poseCallback(
        const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg);

    void imuCallback(
        const sensor_msgs::msg::Imu::SharedPtr msg);

    void gnssCallback(
        const sensor_msgs::msg::NavSatFix::SharedPtr msg);

    void timerCallback();

    //---------------------------------
    // WebSocket
    //---------------------------------

    void connect();

    bool sendJson(const nlohmann::json& json);

    //---------------------------------
    // JSON
    //---------------------------------

    nlohmann::json buildJson();
};