# green_detector.launch.py
from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    container = ComposableNodeContainer(
        name='green_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            ComposableNode(
                package='green_detector',
                plugin='green_detector::GreenDetectorNode',
                name='green_detector',
                parameters=[{
                    'input_topic': '/image_raw',
                    'serial_port': '/dev/ttyACM2',
                    'baudrate': 115200,
                    'fx': 1800.0, 'fy': 1800.0,
                    'cx': 720.0,  'cy': 540.0,
                    'px_offset': 0.0, 'py_offset': 0.0,
                    'min_area': 100.0, 'min_circularity': 0.7,
                    'gray_thresh': 60, 'subgreen_thresh': 60,
                    'yaw_limit_deg': 30.0, 'pitch_limit_deg': 30.0,
                    'target_timeout_s': 0.15,
                    'invert_yaw': False, 'invert_pitch': False,
                }],
                # 跨进程了，intra_process 不再生效，明确设 False
                extra_arguments=[{'use_intra_process_comms': False}]
            ),
        ],
        output='screen',
    )
    return LaunchDescription([container])