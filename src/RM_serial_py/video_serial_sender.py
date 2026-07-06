# """
# 仿照雷达发送示例，通过 0x0310 命令码以 ~5Hz 向裁判系统串口发送 150B 视频数据包。
# 用法：直接运行，或作为模块被 ROS2 节点调用。
# """

# import time
# import serial
# from ser_api import build_send_packet, build_data_video_packet


# def main():
#     ser = serial.Serial('COM19', 115200, timeout=1)  # 根据实际串口号修改
#     seq = 0

#     # 这里用全零模拟 150 字节视频负载，实际使用时替换为真实数据
#     dummy_payload = bytes(150)

#     print("开始以 ~5Hz 发送 0x0310 视频数据包...")

#     while True:
#         data = build_data_video_packet(dummy_payload)
#         packet, seq = build_send_packet(data, seq, [0x03, 0x10])
#         ser.write(packet)
#         time.sleep(0.2)  # 5Hz


# if __name__ == '__main__':
#     main()

"""
video_serial_sender.py
将视频帧分片后, 通过 0x0310 命令码按裁判系统协议串口发送。
整帧发送频率 ~5Hz, 帧内各分片连续发送, 分片间隔 2ms 防止串口拥塞。
"""

import time
import serial
from ser_api import (
    build_send_packet,
    fragment_frame,
)


def main():
    # ---- 串口配置 ----
    ser = serial.Serial('/dev/hero', 115200, timeout=1)
    seq = 0
    frame_id = 0

    print("开始以 ~5Hz 发送 0x0310 视频分片数据包...")

    while True:
        # ---- 获取一帧数据 ----
        # 实际使用时替换为真实的编码帧, 例如:
        #   ret, frame = cap.read()
        #   _, jpeg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 30])
        #   frame_data = jpeg.tobytes()
        frame_data = bytes(1000)  # 模拟 1000 字节, 会被拆成 4 个分片

        # ---- 分片 ----
        fragments = fragment_frame(frame_id, frame_data)
        print(f"帧 {frame_id}: {len(frame_data)}B -> {len(fragments)} 个分片")

        # ---- 逐个分片发送 ----
        for frag_data in fragments:
            packet, seq = build_send_packet(frag_data, seq, [0x03, 0x10])
            ser.write(packet)
            time.sleep(0.002)  # 分片间隔 2ms, 防止串口缓冲区溢出

        # ---- 帧计数 ----
        frame_id = (frame_id + 1) & 0xFFFF  # 0~65535 循环

        # ---- 帧间隔 ----
        time.sleep(0.2)  # ~5Hz


if __name__ == '__main__':
    main()