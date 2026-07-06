#include "green_detector/green_detector_node.hpp"
#include <cv_bridge/cv_bridge.h>
#include <rclcpp_components/register_node_macro.hpp>

using namespace std::chrono_literals;

namespace green_detector
{

GreenDetectorNode::GreenDetectorNode(const rclcpp::NodeOptions & options)
: Node("green_detector_node", options)
{
  fx_ = declare_parameter("fx", 1800.0);
  fy_ = declare_parameter("fy", 1800.0);
  cx_ = declare_parameter("cx", 720.0);
  cy_ = declare_parameter("cy", 540.0);

  px_offset_ = declare_parameter("px_offset", 0.0);
  py_offset_ = declare_parameter("py_offset", 0.0);

  gray_thresh_     = declare_parameter("gray_thresh", 60);
  subgreen_thresh_ = declare_parameter("subgreen_thresh", 60);
  min_area_        = declare_parameter("min_area", 100.0);
  min_circularity_ = declare_parameter("min_circularity", 0.7);

  yaw_limit_deg_    = declare_parameter("yaw_limit_deg", 30.0);
  pitch_limit_deg_  = declare_parameter("pitch_limit_deg", 30.0);
  target_timeout_s_ = declare_parameter("target_timeout_s", 0.15);
  invert_yaw_       = declare_parameter("invert_yaw", false);
  invert_pitch_     = declare_parameter("invert_pitch", false);
  show_debug_       = declare_parameter("show_debug", true);

  auto input_topic = declare_parameter("input_topic", std::string("/image_raw"));
  auto port        = declare_parameter("serial_port", std::string("/dev/ttyUSB0"));
  int  baud        = declare_parameter("baudrate", 115200);

  serial_ = std::make_unique<GimbalSerial>(port, baud);
  if (!serial_->is_open()) {
    RCLCPP_ERROR(get_logger(), "无法打开云台串口 %s，节点运行但不发送", port.c_str());
  } else {
    RCLCPP_INFO(get_logger(), "云台串口已打开 %s @ %d", port.c_str(), baud);
  }

  image_sub_ = create_subscription<sensor_msgs::msg::Image>(
    input_topic, rclcpp::SensorDataQoS(),
    std::bind(&GreenDetectorNode::image_callback, this, std::placeholders::_1));

  target_pub_      = create_publisher<geometry_msgs::msg::Vector3>("green_target", 10);
  send_status_pub_ = create_publisher<geometry_msgs::msg::Vector3>("gimbal_send_status", 10);

  timer_ = create_wall_timer(20ms, std::bind(&GreenDetectorNode::timer_callback, this));

  last_seen_ = std::chrono::steady_clock::now();
  RCLCPP_INFO(get_logger(), "GreenDetectorNode ready, sub=%s", input_topic.c_str());
}

cv::Mat GreenDetectorNode::preprocess(const cv::Mat & bgr)
{
  std::vector<cv::Mat> ch;
  cv::split(bgr, ch);
  cv::Mat sub_green;
  cv::subtract(ch[1], ch[2], sub_green);

  cv::Mat gray;
  cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

  cv::Mat bright, greenish, result;
  cv::threshold(gray, bright, gray_thresh_, 255, cv::THRESH_BINARY);
  cv::threshold(sub_green, greenish, subgreen_thresh_, 255, cv::THRESH_BINARY);
  cv::bitwise_and(bright, greenish, result);

  cv::Mat ff = result.clone();
  cv::floodFill(ff, cv::Point(0, 0), cv::Scalar(255));
  cv::Mat ff_inv;
  cv::bitwise_not(ff, ff_inv);
  result = result | ff_inv;
  return result;
}

bool GreenDetectorNode::detect_center(const cv::Mat & bgr, cv::Point2f & center, cv::Mat & mask_out)
{
  cv::Mat mask = preprocess(bgr);
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3});
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
  mask_out = mask;

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  double best_circ = min_circularity_;
  bool found = false;
  for (const auto & c : contours) {
    double area = cv::contourArea(c);
    if (area < min_area_ || c.size() < 5) continue;
    double peri = cv::arcLength(c, true);
    if (peri <= 0) continue;
    double circ = 4.0 * CV_PI * area / (peri * peri);
    if (circ > best_circ) {
      center = cv::fitEllipse(c).center;
      best_circ = circ;
      found = true;
    }
  }
  return found;
}

void GreenDetectorNode::image_callback(const sensor_msgs::msg::Image::SharedPtr msg)
{
  cv::Mat bgr;
  try {
    bgr = cv_bridge::toCvShare(msg, "bgr8")->image;
  } catch (const cv_bridge::Exception & e) {
    RCLCPP_ERROR(get_logger(), "cv_bridge: %s", e.what());
    return;
  }

  cv::Point2f center;
  cv::Mat mask;
  bool found = detect_center(bgr, center, mask);

  double yaw = 0.0, pitch = 0.0;
  geometry_msgs::msg::Vector3 tmsg;   // 识别: x=yaw y=pitch z=found
  geometry_msgs::msg::Vector3 smsg;   // 发送状态: x=发出yaw y=发出pitch z=状态
  smsg.z = 0.0;

  if (found) {
    double xc = static_cast<double>(center.x) - px_offset_;
    double yc = static_cast<double>(center.y) - py_offset_;
    yaw   = std::atan2((xc - cx_) / fx_, 1.0) * 180.0 / CV_PI;
    pitch = std::atan2((cy_ - yc) / fy_, 1.0) * 180.0 / CV_PI;

    if (invert_yaw_)   yaw   = -yaw;
    if (invert_pitch_) pitch = -pitch;

    if (!std::isfinite(yaw) || !std::isfinite(pitch) ||
        std::abs(yaw) > yaw_limit_deg_ || std::abs(pitch) > pitch_limit_deg_) {
      RCLCPP_WARN(get_logger(), "角度异常丢弃 yaw=%.2f pitch=%.2f", yaw, pitch);
      found = false;
      smsg.z = 0.0;
    } else {
      last_seen_ = std::chrono::steady_clock::now();
      smsg.x = yaw;
      smsg.y = pitch;
      if (serial_ && serial_->is_open()) {
        serial_->send(static_cast<float>(pitch), static_cast<float>(yaw), 1);
        smsg.z = 1.0;
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000,
                             "SEND OK yaw=%.2f pitch=%.2f", yaw, pitch);
      } else {
        smsg.z = -1.0;
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                             "SERIAL NOT OPEN, yaw=%.2f pitch=%.2f", yaw, pitch);
      }
    }
  }   // <<< 这里就是补上的、用来关闭 if (found) 的括号

  tmsg.x = found ? yaw   : 0.0;
  tmsg.y = found ? pitch : 0.0;
  tmsg.z = found ? 1.0   : 0.0;
  target_pub_->publish(tmsg);
  send_status_pub_->publish(smsg);

  if (show_debug_) show(bgr, mask, found, center, yaw, pitch);
}

void GreenDetectorNode::timer_callback()
{
  if (!serial_ || !serial_->is_open()) return;

  serial_->tryReceive();
  if (serial_->hasRxData()) {
    float fb = serial_->takeRxYaw();          // 取走并复位
    RCLCPP_DEBUG(get_logger(), "gimbal fb yaw=%.2f", fb);
  }

  auto now = std::chrono::steady_clock::now();
  double dt = std::chrono::duration<double>(now - last_seen_).count();
  if (dt > target_timeout_s_) {
    serial_->sendIdle();
  }
}

void GreenDetectorNode::show(const cv::Mat & bgr, const cv::Mat & mask,
                             bool found, const cv::Point2f & center,
                             double yaw, double pitch)
{
  cv::Mat vis = bgr.clone();

  cv::Point2f axis(static_cast<float>(cx_ + px_offset_),
                   static_cast<float>(cy_ + py_offset_));
  cv::drawMarker(vis, axis, {255, 255, 0}, cv::MARKER_CROSS, 30, 1);

  if (found) {
    cv::circle(vis, center, 14, {0, 0, 255}, 2, cv::LINE_AA);
    cv::drawMarker(vis, center, {0, 0, 255}, cv::MARKER_CROSS, 24, 2);
    cv::line(vis, axis, center, {0, 255, 255}, 1, cv::LINE_AA);
    char txt[128];
    std::snprintf(txt, sizeof(txt), "yaw=%.2f pitch=%.2f", yaw, pitch);
    cv::putText(vis, txt, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 255, 0}, 2);
  } else {
    cv::putText(vis, "NO TARGET", {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 0, 255}, 2);
  }

  cv::imshow("green_detect", vis);
  if (!mask.empty()) cv::imshow("green_mask", mask);
  cv::waitKey(1);
}

}  // namespace green_detector

RCLCPP_COMPONENTS_REGISTER_NODE(green_detector::GreenDetectorNode)