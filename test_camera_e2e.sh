#!/bin/bash
# test_camera_e2e.sh — 相机 -> 编码 -> 分片 -> 串口 -> 接收重组 -> 显示
# 使用虚拟串口，需要相机或 ROS2 图像 topic
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
WS_DIR="$HOME/sniper_ws"

VSERIAL0="/tmp/vserial0"
VSERIAL1="/tmp/vserial1"

echo "============================================"
echo "  第四层：相机端到端测试"
echo "  相机 -> JPEG -> 分片 -> 串口 -> 重组 -> 显示"
echo "============================================"
echo ""

# 检查依赖
for cmd in socat python3; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ 缺少依赖: $cmd"
        exit 1
    fi
done

# 清理旧进程
echo "[1/5] 清理旧进程..."
pkill -f "socat.*vserial" 2>/dev/null || true
pkill -f "test_camera_e2e.py" 2>/dev/null || true
sleep 0.5
rm -f "$VSERIAL0" "$VSERIAL1"

# 创建虚拟串口
echo "[2/5] 创建虚拟串口对..."
socat -d -d pty,raw,echo=0,link=$VSERIAL0 pty,raw,echo=0,link=$VSERIAL1 &
SOCAT_PID=$!
sleep 1

if [ ! -e "$VSERIAL0" ] || [ ! -e "$VSERIAL1" ]; then
    echo "❌ 虚拟串口创建失败"
    exit 1
fi
echo "   $VSERIAL0 <-> $VSERIAL1 (PID: $SOCAT_PID)"

# 检查相机
echo "[3/5] 检查相机..."
if ls /dev/video* &>/dev/null; then
    echo "   检测到本地摄像头: $(ls /dev/video*)"
    CAM_MODE="local"
else
    echo "   未检测到本地摄像头，将检查 ROS2 图像 topic..."
    CAM_MODE="ros2"
fi

# source ROS2
if [ -f "$WS_DIR/install/setup.bash" ]; then
    source "$WS_DIR/install/setup.bash"
fi

echo "[4/5] 启动端到端测试..."
echo "   按 'q' 键退出显示窗口"
echo ""

cd "$PROJECT_DIR"

# 清理函数
cleanup() {
    echo ""
    echo "[5/5] 清理..."
    kill $SOCAT_PID 2>/dev/null || true
    rm -f "$VSERIAL0" "$VSERIAL1"
    echo "   已清理"
}
trap cleanup EXIT

python3 test_camera_e2e.py