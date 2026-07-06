#include "odomPublisher.hpp"
#include <memory>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<odomPublisher>("odom"));
  rclcpp::shutdown();
  return 0;
}