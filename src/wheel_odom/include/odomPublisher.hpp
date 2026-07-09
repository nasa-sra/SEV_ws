#pragma once
#include "UdpSocket.h"
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/quaternion.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <rclcpp/clock.hpp>
#include <rclcpp/time.hpp>
#include <rclcpp/time_source.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <cmath>
#include <algorithm>
#include <inttypes.h>
#include <mutex>
#include <stop_token>
#include <thread>
#include <tinyxml2.h>
#include "ament_index_cpp/get_package_share_directory.hpp"

class odomPublisher : public rclcpp::Node{

    public:
        odomPublisher(std::string topic_name);
        ~odomPublisher();
        //nav_msgs::msg::Odometry getOdomData();
        void timer_callback();
        void listenLoop(std::stop_token stop_token);
        void processPacket(char* buffer, int length);
        uint16_t loadUdpPort(const std::string& filename);

    private:
        std::string udp_string;
        uint8_t udp_size;
        uint16_t udp_port = 12345;

        char* server_address_str =  (char *)"127.0.0.1";
        uint16_t server_port =  44332;

        std::string topic_name = "odom";
        std::jthread data_port;
        gsc::UdpSocket udp_socket;
        SocketAddress socket_address;


        rclcpp::TimerBase::SharedPtr timer_;
        rclcpp::Time timelocal;
        rclcpp::Time last_time;
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr publisher_;
        size_t count_;


        tf2::Quaternion q_tf;
        geometry_msgs::msg::Quaternion q_msg;

        nav_msgs::msg::Odometry odom;
        geometry_msgs::msg::TransformStamped odom_trans;
        tf2_ros::TransformBroadcaster odom_broadcaster = tf2_ros::TransformBroadcaster(*this);

        std::mutex odom_mutex;
        nav_msgs::msg::Odometry currentOdom;
};