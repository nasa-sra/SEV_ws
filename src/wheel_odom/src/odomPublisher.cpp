#include "odomPublisher.hpp"

using namespace std::chrono_literals;

odomPublisher::odomPublisher(std::string topic_name) : 
    Node("odomPublisher")
    //udp_socket(udp_port)
{

    if (!udp_socket.isReady())
    {
        throw std::runtime_error("Failed to create UDP socket");
    }


    if (!udp_socket.setLocalAddress(udp_port))
    {
        throw std::runtime_error("Could not bind UDP socket");
    }

    udp_socket.setReceiveTimeout(0.1f);

    // socket_address.loadAddress(
    //     gsc::SocketAddress::SA_TYPE_UDP,
    //     gsc::SocketAddress::SA_FAMILY_IPV4,
    //     server_address_str,
    //     server_port              // destination port
    // ); unnecessary, we are not sending anything, only recieving 

    timelocal = this->get_clock()->now();
    last_time = this->get_clock()->now();

    data_port = std::jthread(&odomPublisher::listenLoop, this);

    publisher_ = 
    create_publisher<nav_msgs::msg::Odometry>(
        topic_name, 
        10 
    );
    timer_callback();

    timer_ = this->create_wall_timer(
        500ms, 
        std::bind(&odomPublisher::timer_callback, this)
    );
}

odomPublisher::~odomPublisher() {
    data_port.request_stop();
    data_port.join();
    printf("Class deconstructed");
}

void odomPublisher::listenLoop(std::stop_token stop_token)
{
    char buffer[1024];

    while (!stop_token.stop_requested())
    {
        int n = udp_socket.recvFrom(
            buffer,
            sizeof(buffer),
            socket_address);

        if (n > 0)
        {
            RCLCPP_INFO(
                this->get_logger(),
                "Received %d bytes",
                n);
            processPacket(buffer, n);
        }
        else if (n < 0)
        {
            RCLCPP_WARN(
                this->get_logger(),
                "UDP receive error %d",
                n);
        }

    }
}



void odomPublisher::processPacket(char* buffer, int length)
{
    nav_msgs::msg::Odometry odom;

    // Decode UDP packet
    // Fill odom fields

    std::lock_guard<std::mutex> lock(odom_mutex);

    current_odom = odom;
}

nav_msgs::msg::Odometry odomPublisher::calculateOdometry()
{
    // Implementation for calculating odometry
    return current_odom;
}

void odomPublisher::timer_callback()
{
    nav_msgs::msg::Odometry publishedData = calculateOdometry();
    //RCLCPP_INFO(this->get_logger(), "Publishing odom data from time mark: %" PRIu32, publishedData.header.stamp.sec);
    publisher_->publish(publishedData);
}