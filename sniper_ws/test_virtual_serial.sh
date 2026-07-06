#!/bin/bash
# test_virtual_serial.sh — 用 socat 创建虚拟串口对，验证串口收发+重组
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

VSERIAL0="/tmp/vserial0"
VSERIAL1="/tmp/vserial1"

echo "============================================"
echo "  第二层：虚拟串口对测"
echo "  使用 socat 创建虚拟串口，无需硬件"
echo "============================================"
echo ""

# 检查 socat
if ! command -v socat &> /dev/null; then
    echo "socat 未安装，正在安装..."
    sudo apt update && sudo apt install -y socat
fi

# 清理旧的 socat 进程和虚拟串口
echo "[1/4] 清理旧进程..."
pkill -f "socat.*vserial" 2>/dev/null || true
sleep 0.5
rm -f "$VSERIAL0" "$VSERIAL1"

# 启动 socat
echo "[2/4] 创建虚拟串口对: $VSERIAL0 <-> $VSERIAL1"
socat -d -d pty,raw,echo=0,link=$VSERIAL0 pty,raw,echo=0,link=$VSERIAL1 &
SOCAT_PID=$!
sleep 1

# 检查虚拟串口是否创建成功
if [ ! -e "$VSERIAL0" ] || [ ! -e "$VSERIAL1" ]; then
    echo "❌ 虚拟串口创建失败"
    kill $SOCAT_PID 2>/dev/null
    exit 1
fi
echo "   socat PID: $SOCAT_PID"

# 运行测试
echo "[3/4] 运行虚拟串口测试..."
echo ""
cd "$PROJECT_DIR"

python3 test_virtual_serial.py
EXIT_CODE=$?

# 清理
echo ""
echo "[4/4] 清理..."
kill $SOCAT_PID 2>/dev/null || true
rm -f "$VSERIAL0" "$VSERIAL1"

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ 虚拟串口测试通过"
else
    echo "❌ 虚拟串口测试失败"
fi

exit $EXIT_CODE