"""
video_serial_bridge.py
订阅 /video_stream (VideoPacket 150B) -> 直接包进 0x0310 -> 串口发送
不做任何解码/重编码，零延迟转发
"""

import sys
import time
from pathlib import Path

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from doorlock_sniper.msg import VideoPacket
import serial

sys.path.insert(0, str(Path(__file__).parent))
from ser_api import build_send_packet


class VideoSerialBridge(Node):
    def __init__(self):
        super().__init__('video_serial_bridge')

        self.declare_parameter('serial_port', '/dev/hero')
        self.declare_parameter('serial_baud', 115200)
        self.declare_parameter('topic', '/video_stream')

        serial_port = self.get_parameter('serial_port').value
        serial_baud = self.get_parameter('serial_baud').value
        topic = self.get_parameter('topic').value

        # 打开串口
        try:
            self.ser = serial.Serial(serial_port, serial_baud, timeout=1)
            self.get_logger().info(f'串口已打开: {serial_port} @ {serial_baud}')
        except serial.SerialException as e:
            self.get_logger().error(f'串口打开失败: {e}')
            self.ser = None
            return

        self.seq = 0
        self.packet_count = 0
        self.byte_count = 0
        self.start_time = time.time()

        # 订阅 VideoPacket
        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=100
        )
        self.sub = self.create_subscription(
            VideoPacket, topic, self.callback, qos
        )

        self.get_logger().info(
            f'桥接启动: {topic} (150B) -> 0x0310 -> {serial_port}'
        )

    def callback(self, msg):
        if self.ser is None:
            return

        # VideoPacket.data 就是 150 字节的 H.264 分片
        # 直接作为 0x0310 的 data 字段发出去
        data = bytes(msg.data)

        packet, self.seq = build_send_packet(data, self.seq, [0x03, 0x10])

        try:
            self.ser.write(packet)
        except serial.SerialException as e:
            self.get_logger().error(f'串口写入失败: {e}')
            return

        self.packet_count += 1
        self.byte_count += len(packet)

        # 每 200 包打印一次统计
        if self.packet_count % 200 == 0:
            elapsed = time.time() - self.start_time
            rate = self.byte_count / elapsed / 1024
            self.get_logger().info(
                f'已发送 {self.packet_count} 包 | '
                f'{self.byte_count/1024:.1f}kB | '
                f'平均 {rate:.2f}kB/s'
            )

    def destroy_node(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = VideoSerialBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()