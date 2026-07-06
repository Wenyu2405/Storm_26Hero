#pragma once
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <geometry_msgs/msg/vector3.hpp>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <memory>
#include "green_detector/gimbal_serial.hpp"

namespace green_detector
{
class GreenDetectorNode : public rclcpp::Node
{
public:
  explicit GreenDetectorNode(const rclcpp::NodeOptions & options);

private:
  void image_callback(const sensor_msgs::msg::Image::SharedPtr msg);
  void timer_callback();
  cv::Mat preprocess(const cv::Mat & bgr);
  bool detect_center(const cv::Mat & bgr, cv::Point2f & center, cv::Mat & mask_out);
  void show(const cv::Mat & bgr, const cv::Mat & mask,
            bool found, const cv::Point2f & center,
            double yaw, double pitch);

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3>::SharedPtr target_pub_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3>::SharedPtr send_status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::unique_ptr<GimbalSerial> serial_;

  double fx_, fy_, cx_, cy_;
  double px_offset_, py_offset_;
  int gray_thresh_, subgreen_thresh_;
  double min_area_, min_circularity_;
  double yaw_limit_deg_, pitch_limit_deg_;
  double target_timeout_s_;
  bool invert_yaw_, invert_pitch_;
  bool show_debug_;

  std::chrono::steady_clock::time_point last_seen_;
};
}  // namespace green_detector