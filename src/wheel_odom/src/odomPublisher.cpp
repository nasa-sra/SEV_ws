#include "odomPublisher.hpp"

using namespace std::chrono_literals;

odomPublisher::odomPublisher(std::string topic_name) : 
    Node("odomPublisher")
    //udp_socket(udp_port)
{

    std::string config_path =
    ament_index_cpp::get_package_share_directory("wheel_odom")
    + "/config/udp_config.xml";

    udp_port = loadUdpPort(config_path);

    if (!udp_socket.isReady())
    {
        throw std::runtime_error("Failed to create UDP socket");
    }


    if (!udp_socket.setLocalAddress(udp_port))
    {
        throw std::runtime_error("Could not bind UDP socket");
    }

    RCLCPP_INFO(
    this->get_logger(),
    "Listening on UDP port %d",
    udp_port
    );

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


uint16_t odomPublisher::loadUdpPort(const std::string& filename)
{
    tinyxml2::XMLDocument doc;

    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error("Failed to load config file.");
    }

    auto* root = doc.FirstChildElement("config");
    if (!root)
    {
        throw std::runtime_error("Missing <config> element.");
    }

    auto* portElement = root->FirstChildElement("udp_port");
    if (!portElement)
    {
        throw std::runtime_error("Missing <udp_port> element.");
    }

    int port;
    if (portElement->QueryIntText(&port) != tinyxml2::XML_SUCCESS)
    {
        throw std::runtime_error("Invalid UDP port.");
    }

    return static_cast<uint16_t>(port);
}



void odomPublisher::processPacket(char* buffer, int length)
{

    nav_msgs::msg::Odometry odom;
    timelocal = this->get_clock()->now();
    const int NUM_FLOATS = 17;
    constexpr size_t MSG17_SIZE = static_cast<size_t>(17) * sizeof(float);
    //constexpr size_t MSG34_SIZE = 34 * sizeof(float);

    if (length != MSG17_SIZE) {
        RCLCPP_WARN(this->get_logger(), "Unexpected packet size: %d bytes. Expected exactly %zu bytes.s",
                    length, MSG17_SIZE);
        return;
    }

    float data[NUM_FLOATS];
    std::memcpy(data, buffer, sizeof(data));

    const float* raw_data = reinterpret_cast<const float*>(data);



    float LS_X_POS    = raw_data[0];
    float LS_Y_POS    = raw_data[1];
    float LS_Z_POS    = raw_data[2];
    float LS_ROLL_POS = raw_data[3];
    float LS_PITCH_POS = raw_data[4];
    float LS_YAW_POS  = raw_data[5];
    float LS_X_VEL    = raw_data[6];
    float LS_Y_VEL    = raw_data[7];
    float LS_Z_VEL    = raw_data[8];
    float LS_ROLL_VEL = raw_data[9];
    float LS_PITCH_VEL= raw_data[10];
    float LS_YAW_VEL  = raw_data[11];

    odom_trans.header.stamp = timelocal;
    odom_trans.header.frame_id = "odom";
    odom_trans.child_frame_id = "base_link";

    q_tf.setRPY(LS_ROLL_POS, LS_PITCH_POS, LS_YAW_POS);
    q_msg = tf2::toMsg(q_tf);

    odom_trans.transform.translation.x = LS_X_POS;
    odom_trans.transform.translation.y = LS_Y_POS;
    odom_trans.transform.translation.z = LS_Z_POS;
    odom_trans.transform.rotation = q_msg;

    odom_broadcaster.sendTransform(odom_trans);

    odom.pose.covariance.fill(0.0);

    for (size_t i = 0; i < 6; ++i) {
        odom.pose.covariance[i * 7] = 0.001;
    }

    odom.pose.pose.position.x = LS_X_POS;
    odom.pose.pose.position.y = LS_Y_POS;
    odom.pose.pose.position.z = LS_Z_POS;
    odom.pose.pose.orientation = q_msg;


    odom.header.stamp = timelocal;
    odom.header.frame_id = "odom";
    odom.child_frame_id = "base_link";


    odom.twist.twist.linear.x = LS_X_VEL;
    odom.twist.twist.linear.y = LS_Y_VEL;
    odom.twist.twist.linear.z = LS_Z_VEL;

    odom.twist.twist.angular.x = LS_ROLL_VEL;
    odom.twist.twist.angular.y = LS_PITCH_VEL;
    odom.twist.twist.angular.z = LS_YAW_VEL;

    RCLCPP_INFO(this->get_logger(),
                "Received odom data: Position (%.3f, %.3f, %.3f), Orientation (%.3f, %.3f, %.3f), Linear Velocity (%.3f, %.3f, %.3f), Angular Velocity (%.3f, %.3f, %.3f)",
                LS_X_POS, LS_Y_POS, LS_Z_POS,
                LS_ROLL_POS, LS_PITCH_POS, LS_YAW_POS,
                LS_X_VEL, LS_Y_VEL, LS_Z_VEL,
                LS_ROLL_VEL, LS_PITCH_VEL, LS_YAW_VEL);
    std::lock_guard<std::mutex> lock(odom_mutex);
    currentOdom = odom;
}



void odomPublisher::timer_callback()
{
    nav_msgs::msg::Odometry publishedData = currentOdom;
    //RCLCPP_INFO(this->get_logger(), "Publishing odom data from time mark: %" PRIu32, publishedData.header.stamp.sec);
    publisher_->publish(publishedData);
}