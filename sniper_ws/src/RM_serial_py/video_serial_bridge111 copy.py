"""
video_serial_bridge.py
订阅 /video_stream (VideoPacket 150B)
-> 加交互头 -> 包进 0x0310 -> 串口发送给裁判系统
"""

import sys
import struct
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
        self.declare_parameter('robot_id', 1)           # 你的机器人 ID
        self.declare_parameter('client_id', 0x0103)     # 对应客户端 ID
        self.declare_parameter('data_cmd_id', 0x0200)   # 自定义子命令码

        serial_port = self.get_parameter('serial_port').value
        serial_baud = self.get_parameter('serial_baud').value
        topic = self.get_parameter('topic').value
        self.robot_id = self.get_parameter('robot_id').value
        self.client_id = self.get_parameter('client_id').value
        self.data_cmd_id = self.get_parameter('data_cmd_id').value

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

        # 预构建交互头（每包都一样，不用每次重新算）
        self.interaction_header = struct.pack('<HHH',
            self.data_cmd_id,   # 自定义子命令码
            self.robot_id,      # 发送者 ID
            self.client_id      # 接收者客户端 ID
        )

        self.get_logger().info(
            f'交互头: cmd=0x{self.data_cmd_id:04X} '
            f'robot={self.robot_id} client=0x{self.client_id:04X}'
        )

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
            f'桥接启动: {topic} -> [6B头+150B数据] -> 0x0310 -> {serial_port}'
        )

    def callback(self, msg):
        if self.ser is None:
            return

        # 交互头(6B) + VideoPacket数据(150B) = 156B
        data = self.interaction_header + bytes(msg.data)

        packet, self.seq = build_send_packet(data, self.seq, [0x03, 0x10])

        try:
            self.ser.write(packet)
        except serial.SerialException as e:
            self.get_logger().error(f'串口写入失败: {e}')
            return

        self.packet_count += 1
        self.byte_count += len(packet)

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