#!/bin/bash
# ~/sniper_ws/start_sniper.sh

sleep 3

if [ ! -t 0 ]; then
  exec gnome-terminal --title="Sniper" -- bash -c "$0; exec bash"
fi

source /opt/ros/humble/setup.bash
source ~/sniper_ws/install/setup.bash

echo "=== 启动狙击手视频链路 ==="
echo "  相机 + 编码器 (composable container)"
echo "  解码器 (Python)"
echo "  串口发送 → 图传 → 自定义客户端 (0x0310)"
echo ""
echo "Ctrl+C 停止全部"
echo ""

# 终端2：green_detector 独占，只显示它自己的日志
gnome-terminal --title="Green Detector" -- bash -c '
  source /opt/ros/humble/setup.bash
  source ~/sniper_ws/install/setup.bash
  echo "=== green_detector 独立终端 ==="
  sleep 3
  ros2 launch bringup green_detector.launch.py
  exec bash
' &

ros2 launch bringup sniper.launch.py