#!/bin/bash
# test_loopback.sh — 物理串口回环测试，需要 USB-TTL 模块且 TX 接 RX
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "============================================"
echo "  第三层：物理串口回环测试"
echo "  需要: USB-TTL 模块，TX 短接 RX"
echo "============================================"
echo ""

# 自动检测串口
echo "[1/3] 检测串口设备..."
PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)

if [ -z "$PORTS" ]; then
    echo "❌ 未检测到任何串口设备"
    echo "   请插入 USB-TTL 模块，并将 TX 和 RX 用杜邦线短接"
    echo ""
    echo "   接线方式:"
    echo "   TX ──┐"
    echo "        │ (杜邦线)"
    echo "   RX ──┘"
    exit 1
fi

echo "   检测到以下串口:"
echo "$PORTS" | while read p; do echo "     $p"; done
echo ""

# 让用户选择或使用第一个
DEFAULT_PORT=$(echo "$PORTS" | head -n1)
read -p "请输入要使用的串口 [默认: $DEFAULT_PORT]: " USER_PORT
SERIAL_PORT=${USER_PORT:-$DEFAULT_PORT}

if [ ! -e "$SERIAL_PORT" ]; then
    echo "❌ 串口 $SERIAL_PORT 不存在"
    exit 1
fi

# 检查权限
echo "[2/3] 检查串口权限..."
if [ ! -r "$SERIAL_PORT" ] || [ ! -w "$SERIAL_PORT" ]; then
    echo "   权限不足，尝试添加当前用户到 dialout 组..."
    sudo usermod -aG dialout "$USER"
    echo "   已添加，本次使用 sudo 运行"
    echo ""
fi

# 运行测试
echo "[3/3] 运行回环测试 (串口: $SERIAL_PORT)..."
echo ""
cd "$PROJECT_DIR"

# 临时修改串口地址并运行
python3 -c "
import sys
sys.path.insert(0, '.')

# 动态修改端口
import test_serial_loopback
test_serial_loopback.main = lambda: None  # 先禁用

import serial, time, struct, random
from ser_api import build_send_packet, fragment_frame

port = '$SERIAL_PORT'
baud = 115200
ser = serial.Serial(port, baud, timeout=2)
ser.reset_input_buffer()
ser.reset_output_buffer()

random.seed(42)
original_data = bytes([random.randint(0, 255) for _ in range(800)])
frame_id = 123

print(f'原始数据: {len(original_data)} 字节')
fragments = fragment_frame(frame_id, original_data)
print(f'分片数: {len(fragments)}')

seq = 0
for frag in fragments:
    packet, seq = build_send_packet(frag, seq, [0x03, 0x10])
    ser.write(packet)
    time.sleep(0.005)

time.sleep(0.5)
received = ser.read(ser.in_waiting)
print(f'回环收到: {len(received)} 字节')

if len(received) == 0:
    print('❌ 未收到任何数据，请检查 TX 是否短接到 RX')
    sys.exit(1)

# 解析
buf = received
frags_map = {}
count = 0
while len(buf) >= 9:
    if buf[0] != 0xA5:
        buf = buf[1:]
        continue
    data_len = struct.unpack_from('<H', buf, 1)[0]
    total_len = 7 + data_len + 2
    if len(buf) < total_len:
        break
    cmd_id = struct.unpack_from('<H', buf, 5)[0]
    data = buf[7:7 + data_len]
    buf = buf[total_len:]
    if cmd_id == 0x0310 and len(data) >= 6:
        fid, fidx, ftotal, dlen = struct.unpack_from('<HBBH', data, 0)
        payload = data[6:6 + dlen]
        frags_map[fidx] = payload
        count += 1
        print(f'  分片{count}: frame_id={fid}, idx={fidx}/{ftotal}, payload={dlen}B')

if count > 0:
    total = max(frags_map.keys()) + 1
    reassembled = b''.join(frags_map[i] for i in range(total))
    if reassembled == original_data:
        print(f'✅ 重组成功: {len(reassembled)} 字节, 数据一致')
    else:
        print(f'❌ 数据不一致')
        sys.exit(1)
else:
    print('❌ 未解析到有效分片')
    sys.exit(1)

ser.close()
"

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ 物理回环测试通过"
else
    echo "❌ 物理回环测试失败"
fi

exit $EXIT_CODE