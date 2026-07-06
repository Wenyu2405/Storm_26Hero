# sniper_ws — 部署模式低带宽图传 + 绿灯自瞄链路

RoboMaster 部署模式下，英雄机器人把枪口画面用 **H.264 + 串口** 投到自定义客户端，配套绿灯识别 + 云台自瞄。

> 工程 fork 自 [【RM2026】部署模式低带宽落点图传 - 五大湖联合大学](https://github.com/lihanhui/sniper_ws)
> 原项目 [演示视频](https://www.bilibili.com/video/BV12aDMBcEob) ｜ [RM 社区开源报告](https://bbs.robomaster.com/article/1883295)

本仓库的增量改动主要在 `src/green_detector/` 和 `src/doorlock_sniper/`，新增/改动了 `src/doorlock_decoder/` 中的串口发送节点。

---

## 目录

- [系统概览](#系统概览)
- [运行环境](#运行环境)
- [安装依赖](#安装依赖)
- [编译启动](#编译启动)
- [包结构](#包结构)
- [图传链路详解](#图传链路详解)
- [绿灯自瞄节点](#绿灯自瞄节点)
- [串口 / 协议说明](#串口--协议说明)
- [分层测试脚本](#分层测试脚本)
- [常用启动命令](#常用启动命令)
- [调参速查](#调参速查)
- [常见问题](#常见问题)
- [致谢](#致谢)

---

## 系统概览

整条链路分两段，全在 ROS 2 进程内通信：

```
┌──────────────────── 编码端（同进程 Composable） ────────────────────┐
│                                                                       │
│  [Hik 工业相机] ── /image_raw ──▶ [video_encoder]                    │
│                                       │ GStreamer H.264               │
│                                       ▼                               │
│                                  /video_stream  (doorlock_sniper/msg) │
└───────────────────────────────────────────────────────────────────────┘
                                       │ ROS 2 topic
┌───────────────────── 发送 + 解码（独立 Python 节点） ─────────────────┐
│                                                                       │
│  [video_uart_sender] ── 0x0310 分片 ──▶ /dev/hero ──▶ 图传客户端      │
│                                                                       │
│  [video_decoder_node]  ◀── /video_stream  ──▶ cv2.imshow  (本地显示) │
│                                                                       │
│  /image_raw ──▶ [green_detector]                                      │
│                       │ 图像预处理 + 圆度筛选                          │
│                       ▼                                                │
│              /green_target  (yaw, pitch, found)                       │
│                       │ 串口 (自定义协议)                              │
│                       ▼                                                │
│                 /dev/ttyACM2  →  云台 MCU                              │
└───────────────────────────────────────────────────────────────────────┘
```

> `video_uart_sender` 把 H.264 帧切成 300 字节负载的串口包，命令码 **0x0310**，跟原版一致；新增的 `green_detector` 与图传链路完全解耦，可独立运行。

---

## 运行环境

- Ubuntu Linux
- ROS 2 **Humble**（实测；Kilted 也可，QoS API 略有差异）
- 海康 MVS SDK
- 串口：
  - `/dev/hero`（图传链路，921600 baud）
  - `/dev/ttyACM2`（云台控制，115200 baud）
- 至少 1 个 USB-TTL 模块（用于回环测试）

> ⚠️ 串口设备名按自己机器改；下面所有命令里出现的 `/dev/hero`、`/dev/ttyACM2` 都要按实际替换。

---

## 安装依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  python3-colcon-common-extensions python3-rosdep \
  python3-opencv python3-av python3-serial \
  libopencv-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-ugly gstreamer1.0-libav \
  socat
```

初始化 rosdep（首次）：

```bash
sudo rosdep init
rosdep update
```

然后在工作空间根目录：

```bash
rosdep install --from-paths src --ignore-src -r -y
```

`hik_camera` 依赖海康 MVS SDK 的两个路径，需要保证文件齐全：

- 头文件：`/opt/MVS/include`
- 库文件：`/opt/MVS/lib/64`

---

## 编译启动

```bash
# 1) 一次性 source ROS 2
source /opt/ros/humble/setup.bash

# 2) 编译
cd ~/sniper_ws
colcon build

# 3) source 自己的工作空间
source install/setup.bash

# 4) 一键启动（推荐用脚本，会自动开 green_detector 独立终端）
./start_sniper.sh
```

或者手动起：

```bash
# 主链路：相机 + 编码 + 解码 + 串口发送
ros2 launch bringup sniper.launch.py

# 绿灯自瞄（开新终端）
ros2 launch bringup green_detector.launch.py
```

---

## 包结构

```
sniper_ws/
├── README.md                       # ← 你正在看的
├── start_sniper.sh                 # 一键启动（开 green_detector 独立终端）
├── start_video_sender.sh           # 老版本：手动起 3 个节点的脚本
├── loopback_test.py                # 物理串口回环小工具（/dev/hero @921600）
│
├── src/
│   ├── bringup/                    # ★ 启动文件
│   │   └── launch/
│   │       ├── sniper.launch.py           # 相机+编码+解码+串口
│   │       └── green_detector.launch.py   # 绿灯自瞄（独立 Composable）
│   │
│   ├── hik_camera/                 # 海康相机节点（从 rm-vision 改）
│   ├── hik/                        # 海康 MVS SDK（vendor）
│   │
│   ├── doorlock_sniper/            # ★ H.264 编码 Composable 节点
│   │   ├── msg/VideoPacket.msg     # 固定 300 字节负载
│   │   └── src/video_encoder_node.cpp
│   │
│   ├── doorlock_decoder/           # ★ Python 解码 + 串口发送
│   │   └── doorlock_decoder/
│   │       ├── video_decoder_node.py
│   │       └── video_uart_sender.py
│   │
│   ├── green_detector/             # ★ 新增：绿灯识别 + 云台自瞄
│   │   ├── include/green_detector/
│   │   └── src/green_detector_node.cpp
│   │
│   └── RM_serial_py/               # CRC8/CRC16 + 串口封包工具（ser_api.py）
│
└── test_*.sh                       # 5 层测试脚本（见下表）
```

---

## 图传链路详解

### 编码端（`sniper.launch.py` 里 `sniper_container`）

Composable 节点 + intra-process zero-copy：

- `hik_camera::HikCameraNode` —— 海康相机，发布 `/image_raw` (`sensor_msgs/Image`)，`exposure_time=20000`、`gain=10` 起手
- `doorlock_sniper::VideoEncoderNode` —— 拿到图后：
  - 中心裁剪 800×800 ROI（`crop_size`）
  - 缩到 `output_size=300` 喂给 x264
  - 静态简化 + 运动区域掩膜 + 拖尾抑制（`static_simplify`/`motion_*`/`motion_trail_frames`）
  - x264 `veryslow` 预设，目标码率 **10 kB/s**（80 kbps），硬上限 14 kB/s（带宽滑窗 2s）
  - 编码完按 300 字节切包，发布 `doorlock_sniper/msg/VideoPacket`

> 关键参数全部在 `sniper.launch.py` 里，每个都有注释，调试时改这一个文件就行。

### 解码端（独立 Python 进程）

- `video_decoder_node` —— 订阅 `/video_stream`，用 `pyav` 软解 H.264，可选 `cv2.imshow` 显示 + 准星叠加（`crosshair_offset_x/y/width`）
- `video_uart_sender` —— 订阅同一个 topic，按 RM 自定义协议封成 0x0310 分片，丢进 `/dev/hero @ 921600`

两个节点都是 `Node` 形式启动，没有跑在 Composable 容器里——发送端不订阅 intra-process，所以单独跑更省心。

---

## 绿灯自瞄节点

`green_detector::GreenDetectorNode`（独立 Composable 容器，单独终端跑）。

### 图像管线

1. **亮度通道**：`cv::cvtColor(BGR → Gray)` → `threshold(gray, gray_thresh)`
2. **绿度通道**：`split → subtract(green - red)` → `threshold(subgreen_thresh)`
3. `bitwise_and` 得候选 mask
4. `floodFill` 补齐灯杆中心可能的暗孔
5. `morphologyEx(MORPH_OPEN)` 去噪
6. `findContours` + 圆形度筛选（`min_area` / `min_circularity`），取最圆的一个

### 坐标转换

- 像素偏移 → 角度：`yaw = atan2((xc-cx)/fx, 1) * 180/π`，pitch 同理
- 内参参数化：内参矩阵在 ROS 参数里直接传 `fx/fy/cx/cy`
- 准星偏移：`px_offset` / `py_offset`（如果相机光心和枪管光心有偏）
- 角度限幅：`yaw_limit_deg` / `pitch_limit_deg` 超过直接丢弃
- 方向反相：`invert_yaw` / `invert_pitch`

### 串口协议

- 默认端口 `/dev/ttyACM2 @ 115200`
- 连续 `target_timeout_s`（默认 0.15s）没识别到目标，自动发 idle
- 发送时机：每帧识别成功就调 `serial_->send(pitch, yaw, mode=1)`，由 `gimbal_serial` 子模块负责封包

### ROS 话题

| 话题 | 方向 | 类型 | 含义 |
|---|---|---|---|
| `/image_raw` | 订阅 | `sensor_msgs/Image` | 相机原图（`SensorDataQoS`） |
| `/green_target` | 发布 | `geometry_msgs/Vector3` | `x=yaw, y=pitch, z=found(0/1)` |
| `/gimbal_send_status` | 发布 | `geometry_msgs/Vector3` | `x=已发yaw, y=已发pitch, z=状态` |

### 调参

`green_detector.launch.py` 里的 `parameters` 块就是全部可调项。常见的几个：

| 参数 | 默认 | 说明 |
|---|---|---|
| `gray_thresh` | 60 | 亮度阈值，调高 → 灯更"亮"才识别 |
| `subgreen_thresh` | 60 | 绿度阈值，调高 → 抗黄绿干扰 |
| `min_circularity` | 0.7 | 圆度阈值，0.6~0.85 之间试 |
| `min_area` | 100 | 最小面积（像素²），远距离时调小 |
| `fx/fy/cx/cy` | 1800/1800/720/540 | 内参，标定完来填 |
| `target_timeout_s` | 0.15 | 多久没目标就发 idle |
| `show_debug` | true | 节点内置 `cv::imshow` 调试窗口 |

---

## 串口 / 协议说明

### 图传：0x0310 分片

- 命令码 `0x0310`
- 单包结构（参考 `RM_serial_py/ser_api.py` / `doorlock_decoder/video_uart_sender.py`）：
  - 帧头 `0xA5`
  - 长度 + cmd_id（LE）
  - 负载：`frame_id` (u16) + `frag_idx` (u8) + `frag_total` (u8) + `data_len` (u16) + 300B 负载
  - CRC16
- 波特率 **921600**

### 云台：自定义协议

由 `green_detector` 节点内嵌的 `gimbal_serial` 模块封包，格式在本仓库的 `include/green_detector/gimbal_serial.hpp` 里（略）。波特率 **115200**。

### RM 标准 CRC

`RM_serial_py/ser_api.py` 提供了 CRC8 / CRC16 查表实现，可被独立脚本复用。

---

## 分层测试脚本

仓库根目录有 5 个测试脚本，**按编号顺序跑**，前一层通过再走下一层：

| 编号 | 脚本 | 用途 | 依赖 |
|---|---|---|---|
| 1 | `test_unit.sh` | 分片/打包逻辑单元测试 | 无 |
| 2 | `test_virtual_serial.sh` | `socat` 虚拟串口对，验证收发+重组 | `socat` |
| 3 | `test_loopback.sh` | 物理 USB-TTL 回环（TX 接 RX） | USB-TTL |
| 4 | `test_camera_e2e.sh` | 相机→JPEG→分片→串口→重组→显示 | 相机或 `/image_raw` topic |
| 5 | `test_referee.sh` | 真实裁判系统联调，发送 0x0310 | 裁判系统串口 |

快速跑一遍（无硬件）：

```bash
./test_unit.sh
./test_virtual_serial.sh
```

带硬件：

```bash
# TX 和 RX 短接
./test_loopback.sh
# 接好相机
./test_camera_e2e.sh
# 接裁判系统
./test_referee.sh
```

`loopback_test.py` 是一个独立小工具，专门测 `/dev/hero @ 921600` 物理回环，能识别"TX/RX 没短接"和"回环数据不一致"两种错误。

---

## 常用启动命令

| 场景 | 命令 |
|---|---|
| 一键启动 | `./start_sniper.sh` |
| 只跑图传链路 | `ros2 launch bringup sniper.launch.py` |
| 只跑绿灯自瞄 | `ros2 launch bringup green_detector.launch.py` |
| 手动起 3 个节点（老脚本） | `./start_video_sender.sh` |
| 物理回环小工具 | `python3 loopback_test.py` |
| 查看 /image_raw 频率 | `ros2 topic hz /image_raw` |
| 录包 | `ros2 bag record /image_raw /video_stream /green_target` |

`start_sniper.sh` 会自动开两个 `gnome-terminal` 窗口：一个跑主链路，一个跑 `green_detector.launch.py`，方便分别看日志。

---

## 调参速查

### 图传太糊 / 卡顿

1. 改 `sniper.launch.py` 里 `target_bitrate_kbytes`（默认 10）和 `hard_max_bitrate_kbytes`（14）
2. 调 `motion_threshold` / `motion_dilate_px` 让运动区域更聚焦
3. `static_simplify=true` 会让静止帧降码率，记得开

### 绿灯识别飘

1. 把 `show_debug=true`，先看 `green_mask` 窗口里 mask 干不干净
2. `gray_thresh` / `subgreen_thresh` 看环境光再调
3. `min_circularity` 调到 0.6 试试，0.8 太严
4. 确认 `fx/fy/cx/cy` 是当前分辨率的标定值（默认是 1440×1080）

### 串口打开失败

```bash
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/hero
sudo usermod -aG dialout $USER   # 注销重登生效
sudo chmod 666 /dev/hero         # 临时应急
```

### 编码端 CPU 高

- `x264_preset` 从 `veryslow` 调到 `medium`/`fast`
- 降低 `output_fps`
- 确认 `use_intra_process_comms: True` 开着（编码端容器里默认是开的）

---

## 常见问题

**Q: `rosdep install` 报 python3-av 找不到？**
A: `python3-av` 在 universe 仓库，确认 `apt sources` 里有 `universe`。

**Q: 启动报 `MVS` 头文件找不到？**
A: 海康 SDK 没装或路径不对，参考 [环境要求](#运行环境) 一节。

**Q: 串口发送节点卡住？**
A: 检查 `/dev/hero` 是不是被别的进程占了；`lsof /dev/hero` 看一下。

**Q: `green_detector` 节点找不到云台串口？**
A: 默认 `/dev/ttyACM2`，按你机器的串口名改 `green_detector.launch.py` 里的 `serial_port` 参数。

**Q: 怎么录像调试？**
A: `ros2 bag record -o sniper.bag /image_raw /video_stream /green_target /gimbal_send_status`

---

## 致谢

- 原项目作者 [@lihanhui](https://github.com/lihanhui) 及五大湖联合大学战队
- `hik_camera` 改自 [rm-vision](https://github.com/rm-vision)
- 海康 MVS SDK
- 所有 LLM 辅助开发工具
