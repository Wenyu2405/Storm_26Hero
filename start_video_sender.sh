#!/bin/bash
# ~/sniper_ws/start_video_sender.sh

source /opt/ros/humble/setup.bash
source ~/sniper_ws/install/setup.bash

SERIAL_PORT="/dev/hero"
BAUDRATE=921600
MAX_FPS=50

cleanup() {
    echo ""
    echo "正在关闭所有节点..."
    kill $PID_CAM $PID_ENC $PID_UART 2>/dev/null
    wait $PID_CAM $PID_ENC $PID_UART 2>/dev/null
    echo "已全部关闭"
    exit 0
}

trap cleanup SIGINT SIGTERM

echo "=== 启动视频传输链路 ==="
echo "串口: $SERIAL_PORT @ $BAUDRATE"
echo ""

# 1. 相机
echo "[1/3] 启动 HikCamera..."
ros2 run doorlock_sniper hik_camera &
PID_CAM=$!
sleep 2

# 2. H.264 编码器
echo "[2/3] 启动 H.264 编码器..."
ros2 run doorlock_sniper video_encoder &
PID_ENC=$!
sleep 1

# 3. 串口发送
echo "[3/3] 启动串口发送 (cmd_id=0x0310)..."
ros2 run doorlock_decoder video_uart_sender --ros-args \
    -p serial_port:=$SERIAL_PORT \
    -p baudrate:=$BAUDRATE \
    -p max_fps:=$MAX_FPS &
PID_UART=$!

echo ""
echo "=== 全部启动完成，Ctrl+C 停止 ==="
echo "  相机 PID: $PID_CAM"
echo "  编码器 PID: $PID_ENC"
echo "  串口发送 PID: $PID_UART"
echo ""

wait