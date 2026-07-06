cmake_minimum_required(VERSION 3.8)
project(green_detector)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(geometry_msgs REQUIRED)

add_library(green_detector_component SHARED src/green_detector_node.cpp)
target_include_directories(green_detector_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  ${OpenCV_INCLUDE_DIRS})
target_link_libraries(green_detector_component ${OpenCV_LIBS})
ament_target_dependencies(green_detector_component
  rclcpp rclcpp_components sensor_msgs cv_bridge)

ament_target_dependencies(green_detector_component
  rclcpp rclcpp_components sensor_msgs cv_bridge geometry_msgs)
  
rclcpp_components_register_nodes(green_detector_component
  "green_detector::GreenDetectorNode")

install(TARGETS green_detector_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

ament_package()