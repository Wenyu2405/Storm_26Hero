# ~/sniper_ws/loopback_test.py

import serial
import time

PORT = '/dev/hero'
BAUD = 921600

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(0.1)

# 清空缓冲区
ser.reset_input_buffer()
ser.reset_output_buffer()

# 测试1：发简单数据
test_data = b'\xA5\x01\x02\x03\x04\x05'
ser.write(test_data)
ser.flush()
time.sleep(0.1)
echo = ser.read(len(test_data))

print(f'=== 回环测试 ===')
print(f'串口: {PORT} @ {BAUD}')
print(f'发送 {len(test_data)} 字节: {test_data.hex(" ")}')
print(f'收到 {len(echo)} 字节: {echo.hex(" ") if echo else "无"}')

if echo == test_data:
    print('结果: 通过 — 串口收发正常')
elif len(echo) == 0:
    print('结果: 失败 — 没收到任何数据')
    print('  检查: TX和RX是否短接了？串口号对不对？')
elif echo != test_data:
    print('结果: 异常 — 收到的数据和发送的不一致')
    print(f'  期望: {test_data.hex(" ")}')
    print(f'  实际: {echo.hex(" ")}')

# 测试2：发一个完整的309字节帧看看
test_frame = b'\xA5' + b'\x00' * 307 + b'\xFF'
ser.reset_input_buffer()
ser.write(test_frame)
ser.flush()
time.sleep(0.2)
echo2 = ser.read(309)

print(f'\n发送 {len(test_frame)} 字节完整帧')
print(f'收到 {len(echo2)} 字节')
if len(echo2) == 309:
    print('结果: 309字节完整回环通过')
else:
    print(f'结果: 只收到 {len(echo2)} 字节')

ser.close()