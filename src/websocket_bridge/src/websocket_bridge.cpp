#include "websocket_bridge.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

#include <ament_index_cpp/get_package_share_directory.hpp>

#include <tinyxml2.h>

using json = nlohmann::json;

using namespace std::chrono_literals;


// ============================================================
// Constructor
// ============================================================

WebsocketBridge::WebsocketBridge()
    : Node("websocket_bridge"),
      odom_received_(false),
      pose_received_(false),
      imu_received_(false),
      gnss_received_(false),
      connected_(false)
{
    RCLCPP_INFO(
        get_logger(),
        "Starting WebSocket Bridge");
    

    // --------------------------------------------------------
    // Find package directory
    // --------------------------------------------------------

    std::string package_path;

    try
    {
        package_path =
            ament_index_cpp::get_package_share_directory(
                "websocket_bridge");
    }
    catch (const std::exception &e)
    {
        RCLCPP_FATAL(
            get_logger(),
            "Failed to find websocket_bridge package: %s",
            e.what());

        throw;
    }


    // --------------------------------------------------------
    // Build config path
    // --------------------------------------------------------

    std::string config_path =
        package_path +
        "/config/websocket_bridge.xml";


    RCLCPP_INFO(
        get_logger(),
        "Loading configuration from: %s",
        config_path.c_str());


    // --------------------------------------------------------
    // Load XML configuration
    // --------------------------------------------------------

    if (!loadConfig(config_path))
    {
        RCLCPP_FATAL(
            get_logger(),
            "Failed to load WebSocket configuration");

        throw std::runtime_error(
            "Failed to load WebSocket configuration");
    }


    // --------------------------------------------------------
    // Create /odom subscriber
    // --------------------------------------------------------

    odom_sub_ =
        create_subscription<nav_msgs::msg::Odometry>(
            "/odom",
            10,
            std::bind(
                &WebsocketBridge::odomCallback,
                this,
                std::placeholders::_1));

    
    // --------------------------------------------------------
    // Create /vectornav/pose subscriber
    // --------------------------------------------------------

    pose_sub_ =
    create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
        "/vectornav/pose",
        10,
        std::bind(
            &WebsocketBridge::poseCallback,
            this,
            std::placeholders::_1));


    // --------------------------------------------------------
    // Create /vectornav/imu subscriber
    // --------------------------------------------------------

    imu_sub_ =
        create_subscription<sensor_msgs::msg::Imu>(
            "/vectornav/imu",
            10,
            std::bind(
                &WebsocketBridge::imuCallback,
                this,
                std::placeholders::_1));


    // --------------------------------------------------------
    // Create /vectornav/gnss subscriber
    // --------------------------------------------------------

    gnss_sub_ =
        create_subscription<sensor_msgs::msg::NavSatFix>(
            "/vectornav/gnss",
            10,
            std::bind(
                &WebsocketBridge::gnssCallback,
                this,
                std::placeholders::_1));


    // --------------------------------------------------------
    // Calculate timer period
    // --------------------------------------------------------

    if (publish_rate_ <= 0.0)
    {
        RCLCPP_FATAL(
            get_logger(),
            "Publish rate must be greater than zero");

        throw std::runtime_error(
            "Invalid publish rate");
    }


    auto period =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::duration<double>(
                    1.0 / publish_rate_));


    // --------------------------------------------------------
    // Create publishing timer
    // --------------------------------------------------------

    timer_ =
        create_wall_timer(
            period,
            std::bind(
                &WebsocketBridge::timerCallback,
                this));


    // --------------------------------------------------------
    // Connect to WebSocket server
    // --------------------------------------------------------

    connect();


    RCLCPP_INFO(
        get_logger(),
        "WebSocket Bridge started successfully");
}


// ============================================================
// Destructor
// ============================================================

WebsocketBridge::~WebsocketBridge()
{
    RCLCPP_INFO(
        get_logger(),
        "Shutting down WebSocket Bridge");


    // --------------------------------------------------------
    // Stop ROS timer
    // --------------------------------------------------------

    timer_.reset();


    // --------------------------------------------------------
    // Stop WebSocket client
    // --------------------------------------------------------

    try
    {
        client_.stop();
    }
    catch (const std::exception &e)
    {
        RCLCPP_WARN(
            get_logger(),
            "Exception while stopping WebSocket client: %s",
            e.what());
    }


    // --------------------------------------------------------
    // Wait for WebSocket thread
    // --------------------------------------------------------

    if (websocket_thread_.joinable())
    {
        websocket_thread_.join();
    }


    connected_ = false;


    RCLCPP_INFO(
        get_logger(),
        "WebSocket Bridge shutdown complete");
}


// ============================================================
// Load XML configuration
// ============================================================

bool WebsocketBridge::loadConfig(
    const std::string &config_path)
{
    tinyxml2::XMLDocument doc;


    // --------------------------------------------------------
    // Load XML file
    // --------------------------------------------------------

    tinyxml2::XMLError result =
        doc.LoadFile(
            config_path.c_str());


    if (result != tinyxml2::XML_SUCCESS)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Failed to load XML config file: %s",
            config_path.c_str());

        RCLCPP_ERROR(
            get_logger(),
            "TinyXML2 error: %s",
            doc.ErrorStr());

        return false;
    }


    // --------------------------------------------------------
    // Find root element
    // --------------------------------------------------------

    tinyxml2::XMLElement *root =
        doc.FirstChildElement(
            "websocket_bridge");


    if (root == nullptr)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Missing <websocket_bridge> root element");

        return false;
    }


    // ========================================================
    // WebSocket configuration
    // ========================================================

    tinyxml2::XMLElement *websocket =
        root->FirstChildElement(
            "websocket");


    if (websocket == nullptr)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Missing <websocket> element");

        return false;
    }


    // --------------------------------------------------------
    // Get URI
    // --------------------------------------------------------

    tinyxml2::XMLElement *uri =
        websocket->FirstChildElement(
            "uri");


    if (uri == nullptr ||
        uri->GetText() == nullptr)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Missing or empty <uri> element");

        return false;
    }


    websocket_uri_ =
        uri->GetText();


    // ========================================================
    // Publishing configuration
    // ========================================================

    tinyxml2::XMLElement *publishing =
        root->FirstChildElement(
            "publishing");


    if (publishing == nullptr)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Missing <publishing> element");

        return false;
    }


    // --------------------------------------------------------
    // Get publish rate
    // --------------------------------------------------------

    tinyxml2::XMLElement *rate =
        publishing->FirstChildElement(
            "rate");


    if (rate == nullptr)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Missing <rate> element");

        return false;
    }


    if (rate->QueryDoubleText(
            &publish_rate_)
        != tinyxml2::XML_SUCCESS)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Invalid publish rate");

        return false;
    }


    // --------------------------------------------------------
    // Validate publish rate
    // --------------------------------------------------------

    if (publish_rate_ <= 0.0)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Publish rate must be greater than zero");

        return false;
    }


    // --------------------------------------------------------
    // Print loaded configuration
    // --------------------------------------------------------

    RCLCPP_INFO(
        get_logger(),
        "WebSocket URI: %s",
        websocket_uri_.c_str());

    RCLCPP_INFO(
        get_logger(),
        "Publish rate: %.2f Hz",
        publish_rate_);


    return true;
}


// ============================================================
// Odometry callback
// ============================================================

void WebsocketBridge::odomCallback(
    const nav_msgs::msg::Odometry::SharedPtr msg)
{
    std::lock_guard<std::mutex> lock(
        data_mutex_);


    odom_ =
        *msg;


    odom_received_ =
        true;
}


// ============================================================
// VectorNav pose callback
// ============================================================

void WebsocketBridge::poseCallback(
    const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
{
    std::lock_guard<std::mutex> lock(
        data_mutex_);

    pose_ = *msg;

    pose_received_ = true;
}


// ============================================================
// VectorNav IMU callback
// ============================================================

void WebsocketBridge::imuCallback(
    const sensor_msgs::msg::Imu::SharedPtr msg)
{
    std::lock_guard<std::mutex> lock(
        data_mutex_);


    imu_ =
        *msg;


    imu_received_ =
        true;
}


// ============================================================
// GNSS callback
// ============================================================

void WebsocketBridge::gnssCallback(
    const sensor_msgs::msg::NavSatFix::SharedPtr msg)
{
    std::lock_guard<std::mutex> lock(
        data_mutex_);


    gnss_ =
        *msg;


    gnss_received_ =
        true;
}


// ============================================================
// Build JSON
// ============================================================

json WebsocketBridge::buildJson()
{
    // --------------------------------------------------------
    // Make local copies
    //
    // This allows us to release the mutex before doing
    // potentially expensive JSON serialization.
    // --------------------------------------------------------

    nav_msgs::msg::Odometry odom;

    geometry_msgs::msg::PoseWithCovarianceStamped pose;

    sensor_msgs::msg::Imu imu;

    sensor_msgs::msg::NavSatFix gnss;


    {
        std::lock_guard<std::mutex> lock(
            data_mutex_);


        odom =
            odom_;

        pose =
            pose_;

        imu =
            imu_;

        gnss =
            gnss_;
    }


    // --------------------------------------------------------
    // Create root JSON object
    // --------------------------------------------------------

    json j;


    // ========================================================
    // Timestamp
    // ========================================================

    // j["timestamp"]["sec"] =
    //     odom.header.stamp.sec;

    // j["timestamp"]["nanosec"] =
    //     odom.header.stamp.nanosec;


    // ========================================================
    // Odometry
    // ========================================================

    // j["odom"]["frame_id"] =
    //     odom.header.frame_id;

    // j["odom"]["child_frame_id"] =
    //     odom.child_frame_id;


    // --------------------------------------------------------
    // Position
    // --------------------------------------------------------

    j["odom"]["position"]["x"] =
        odom.pose.pose.position.x;

    j["odom"]["position"]["y"] =
        odom.pose.pose.position.y;

    j["odom"]["position"]["z"] =
        odom.pose.pose.position.z;


    // --------------------------------------------------------
    // Orientation
    // --------------------------------------------------------

    j["odom"]["orientation"]["x"] =
        odom.pose.pose.orientation.x;

    j["odom"]["orientation"]["y"] =
        odom.pose.pose.orientation.y;

    j["odom"]["orientation"]["z"] =
        odom.pose.pose.orientation.z;

    j["odom"]["orientation"]["w"] =
        odom.pose.pose.orientation.w;


    // --------------------------------------------------------
    // Linear velocity
    // --------------------------------------------------------

    j["odom"]["twist"]["linear"]["x"] =
        odom.twist.twist.linear.x;

    j["odom"]["twist"]["linear"]["y"] =
        odom.twist.twist.linear.y;

    j["odom"]["twist"]["linear"]["z"] =
        odom.twist.twist.linear.z;


    // --------------------------------------------------------
    // Angular velocity
    // --------------------------------------------------------

    // j["odom"]["twist"]["angular"]["x"] =
    //     odom.twist.twist.angular.x;

    // j["odom"]["twist"]["angular"]["y"] =
    //     odom.twist.twist.angular.y;

    // j["odom"]["twist"]["angular"]["z"] =
    //     odom.twist.twist.angular.z;


    // ========================================================
    // VectorNav Pose
    // ========================================================

    // j["vectornav_pose"]["frame_id"] =
    //     pose.header.frame_id;


    // --------------------------------------------------------
    // Position
    // --------------------------------------------------------

    j["vectornav_pose"]["position"]["x"] =
        pose.pose.pose.position.x;

    j["vectornav_pose"]["position"]["y"] =
        pose.pose.pose.position.y;

    j["vectornav_pose"]["position"]["z"] =
        pose.pose.pose.position.z;


    // --------------------------------------------------------
    // Orientation
    // --------------------------------------------------------

    j["vectornav_pose"]["orientation"]["x"] =
        pose.pose.pose.orientation.x;

    j["vectornav_pose"]["orientation"]["y"] =
        pose.pose.pose.orientation.y;

    j["vectornav_pose"]["orientation"]["z"] =
        pose.pose.pose.orientation.z;

    j["vectornav_pose"]["orientation"]["w"] =
        pose.pose.pose.orientation.w;

    
    //--------------------------------------------------------
    // Covariance
    //--------------------------------------------------------

    // j["vectornav_pose"]["covariance"] =
    // pose.pose.covariance;

    // ========================================================
    // VectorNav IMU
    // ========================================================

    // j["vectornav_imu"]["frame_id"] =
    //     imu.header.frame_id;


    // --------------------------------------------------------
    // Orientation
    // --------------------------------------------------------

    j["vectornav_imu"]["orientation"]["x"] =
        imu.orientation.x;

    j["vectornav_imu"]["orientation"]["y"] =
        imu.orientation.y;

    j["vectornav_imu"]["orientation"]["z"] =
        imu.orientation.z;

    j["vectornav_imu"]["orientation"]["w"] =
        imu.orientation.w;


    // --------------------------------------------------------
    // Angular velocity
    // --------------------------------------------------------

    // j["vectornav_imu"]["angular_velocity"]["x"] =
    //     imu.angular_velocity.x;

    // j["vectornav_imu"]["angular_velocity"]["y"] =
    //     imu.angular_velocity.y;

    // j["vectornav_imu"]["angular_velocity"]["z"] =
    //     imu.angular_velocity.z;


    // --------------------------------------------------------
    // Linear acceleration
    // --------------------------------------------------------

    // j["vectornav_imu"]["linear_acceleration"]["x"] =
    //     imu.linear_acceleration.x;

    // j["vectornav_imu"]["linear_acceleration"]["y"] =
    //     imu.linear_acceleration.y;

    // j["vectornav_imu"]["linear_acceleration"]["z"] =
    //     imu.linear_acceleration.z;


    // ========================================================
    // VectorNav GNSS
    // ========================================================

    // j["vectornav_gnss"]["frame_id"] =
    //     gnss.header.frame_id;
    // j["vectornav_gnss"]["status"]["status"] =
    //     gnss.status.status;

    // --------------------------------------------------------
    // Latitude, Longitude, Altitude
    // --------------------------------------------------------

    j["vectornav_gnss"]["latitude"] =
        gnss.latitude;
    j["vectornav_gnss"]["longitude"] =
        gnss.longitude;
    // j["vectornav_gnss"]["altitude"] =
    //     gnss.altitude;
    

    //--------------------------------------------------------
    // Position covariance
    //--------------------------------------------------------

    // j["vectornav_gnss"]["position_covariance"] =
    //     gnss.position_covariance;
    
    // //--------------------------------------------------------
    // // Position covariance type
    // //--------------------------------------------------------

    // j["vectornav_gnss"]["position_covariance_type"] =
    //     gnss.position_covariance_type;
    

    return j;
}


// ============================================================
// Timer callback
// ============================================================

void WebsocketBridge::timerCallback()
{
    // --------------------------------------------------------
    // Check if all required data has been received
    // --------------------------------------------------------

    {
        std::lock_guard<std::mutex> lock(
            data_mutex_);


        if (!odom_received_ ||
            !pose_received_ ||
            !imu_received_  ||
            !gnss_received_ )
        {
            return;
        }
    }


    // --------------------------------------------------------
    // Don't send if WebSocket is disconnected
    // --------------------------------------------------------

    if (!connected_)
    {
        return;
    }


    // --------------------------------------------------------
    // Build JSON
    // --------------------------------------------------------

    json j =
        buildJson();


    // --------------------------------------------------------
    // Send JSON
    // --------------------------------------------------------

    sendJson(j);
}


// ============================================================
// Connect to WebSocket server
// ============================================================

void WebsocketBridge::connect()
{
    RCLCPP_INFO(
        get_logger(),
        "Connecting to WebSocket server: %s",
        websocket_uri_.c_str());


    // --------------------------------------------------------
    // Configure WebSocket client
    // --------------------------------------------------------

    client_.clear_access_channels(
        websocketpp::log::alevel::all);


    client_.init_asio();


    // --------------------------------------------------------
    // Connection opened callback
    // --------------------------------------------------------

    client_.set_open_handler(
        [this](websocketpp::connection_hdl hdl)
        {
            connection_ =
                hdl;

            connected_ =
                true;


            RCLCPP_INFO(
                get_logger(),
                "WebSocket connection established");
        });


    // --------------------------------------------------------
    // Connection closed callback
    // --------------------------------------------------------

    client_.set_close_handler(
        [this](websocketpp::connection_hdl)
        {
            connected_ =
                false;


            RCLCPP_WARN(
                get_logger(),
                "WebSocket connection closed");
        });


    // --------------------------------------------------------
    // Connection failure callback
    // --------------------------------------------------------

    client_.set_fail_handler(
        [this](websocketpp::connection_hdl hdl)
        {
            connected_ =
                false;


            auto con =
                client_.get_con_from_hdl(
                    hdl);


            RCLCPP_ERROR(
                get_logger(),
                "WebSocket connection failed: %s",
                con->get_ec().message().c_str());
        });


    // --------------------------------------------------------
    // Create connection
    // --------------------------------------------------------

    websocketpp::lib::error_code ec;


    auto connection =
        client_.get_connection(
            websocket_uri_,
            ec);


    if (ec)
    {
        RCLCPP_ERROR(
            get_logger(),
            "Failed to create WebSocket connection: %s",
            ec.message().c_str());

        return;
    }


    // --------------------------------------------------------
    // Store connection handle
    // --------------------------------------------------------

    connection_ =
        connection->get_handle();


    // --------------------------------------------------------
    // Connect
    // --------------------------------------------------------

    client_.connect(
        connection);


    // --------------------------------------------------------
    // Start WebSocket event loop
    // --------------------------------------------------------

    websocket_thread_ =
        std::thread(
            [this]()
            {
                try
                {
                    client_.run();
                }
                catch (const std::exception &e)
                {
                    RCLCPP_ERROR(
                        get_logger(),
                        "WebSocket thread exception: %s",
                        e.what());
                }
            });
}


// ============================================================
// Send JSON over WebSocket
// ============================================================

bool WebsocketBridge::sendJson(
    const json &j)
{
    if (!connected_)
    {
        return false;
    }


    // --------------------------------------------------------
    // Convert JSON object to string
    // --------------------------------------------------------

    std::string payload =
        j.dump();


    // --------------------------------------------------------
    // Send message
    // --------------------------------------------------------

    websocketpp::lib::error_code ec;


    client_.send(
        connection_,
        payload,
        websocketpp::frame::opcode::text,
        ec);


    // --------------------------------------------------------
    // Check for error
    // --------------------------------------------------------

    if (ec)
    {
        RCLCPP_WARN(
            get_logger(),
            "Failed to send WebSocket message: %s",
            ec.message().c_str());


        connected_ =
            false;


        return false;
    }


    return true;
}