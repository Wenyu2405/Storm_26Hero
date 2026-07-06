#!/bin/bash
# test_referee.sh — 连接真实裁判系统，发送 0x0310 数据包
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "============================================"
echo "  第五层：裁判系统联调测试"
echo "  连接真实裁判系统串口，发送 0x0310"
echo "============================================"
echo ""

# 检测串口
echo "[1/3] 检测串口设备..."
PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)

if [ -z "$PORTS" ]; then
    echo "❌ 未检测到串口设备"
    echo "   请连接裁判系统串口线"
    exit 1
fi

echo "   检测到:"
echo "$PORTS" | while read p; do echo "     $p"; done
echo ""

DEFAULT_PORT=$(echo "$PORTS" | head -n1)
read -p "请输入裁判系统串口 [默认: $DEFAULT_PORT]: " USER_PORT
SERIAL_PORT=${USER_PORT:-$DEFAULT_PORT}

# 检查权限
if [ ! -r "$SERIAL_PORT" ] || [ ! -w "$SERIAL_PORT" ]; then
    echo "   串口权限不足，添加 dialout 组..."
    sudo usermod -aG dialout "$USER"
    sudo chmod 666 "$SERIAL_PORT"
fi

echo "[2/3] 向裁判系统发送测试数据..."
echo "   串口: $SERIAL_PORT"
echo "   命令码: 0x0310"
echo ""

cd "$PROJECT_DIR"

python3 -c "
import time, serial, sys
sys.path.insert(0, '.')
from ser_api import build_send_packet, fragment_frame

ser = serial.Serial('$SERIAL_PORT', 115200, timeout=1)
ser.reset_input_buffer()

test_payload = b'REFEREE_0310_TEST_' + bytes(100)
frame_id = 0
seq = 0
total = 20

print(f'发送 {total} 帧测试数据...')
print('-' * 50)

for i in range(total):
    fragments = fragment_frame(frame_id, test_payload)
    for frag in fragments:
        packet, seq = build_send_packet(frag, seq, [0x03, 0x10])
        ser.write(packet)
        time.sleep(0.002)

    time.sleep(0.05)
    resp_len = ser.in_waiting
    if resp_len > 0:
        resp = ser.read(resp_len)
        print(f'  帧{frame_id:3d}: 已发送 {len(fragments)}片, 裁判系统回应 {resp_len}B')
    else:
        print(f'  帧{frame_id:3d}: 已发送 {len(fragments)}片, 无回应')

    frame_id = (frame_id + 1) & 0xFFFF
    time.sleep(0.2)

print('-' * 50)
print('发送完毕, 请在裁判系统客户端确认是否收到数据')
ser.close()
"

echo ""
echo "[3/3] 测试完成"
echo "   请检查裁判系统客户端 UI 是否显示收到图传数据"