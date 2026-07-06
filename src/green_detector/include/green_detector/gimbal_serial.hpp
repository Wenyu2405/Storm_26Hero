#pragma once
/*  gimbal_serial.hpp
 *
 *  发送协议（11字节）：
 *    [0x6A] [pitch 4B] [yaw 4B] [target_found 1B] [crc8 1B] [0xA6]
 *
 *  接收协议（6字节，无CRC）：
 *    [0x6A] [yaw 4B float] [0xA6]
 */

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <string>

// ========== CRC8（仅发送用） ==========
namespace gimbal_crc {

static const uint8_t TABLE[256] = {
  0x00,0x5e,0xbc,0xe2,0x61,0x3f,0xdd,0x83,0xc2,0x9c,0x7e,0x20,0xa3,0xfd,0x1f,0x41,
  0x9d,0xc3,0x21,0x7f,0xfc,0xa2,0x40,0x1e,0x5f,0x01,0xe3,0xbd,0x3e,0x60,0x82,0xdc,
  0x23,0x7d,0x9f,0xc1,0x42,0x1c,0xfe,0xa0,0xe1,0xbf,0x5d,0x03,0x80,0xde,0x3c,0x62,
  0xbe,0xe0,0x02,0x5c,0xdf,0x81,0x63,0x3d,0x7c,0x22,0xc0,0x9e,0x1d,0x43,0xa1,0xff,
  0x46,0x18,0xfa,0xa4,0x27,0x79,0x9b,0xc5,0x84,0xda,0x38,0x66,0xe5,0xbb,0x59,0x07,
  0xdb,0x85,0x67,0x39,0xba,0xe4,0x06,0x58,0x19,0x47,0xa5,0xfb,0x78,0x26,0xc4,0x9a,
  0x65,0x3b,0xd9,0x87,0x04,0x5a,0xb8,0xe6,0xa7,0xf9,0x1b,0x45,0xc6,0x98,0x7a,0x24,
  0xf8,0xa6,0x44,0x1a,0x99,0xc7,0x25,0x7b,0x3a,0x64,0x86,0xd8,0x5b,0x05,0xe7,0xb9,
  0x8c,0xd2,0x30,0x6e,0xed,0xb3,0x51,0x0f,0x4e,0x10,0xf2,0xac,0x2f,0x71,0x93,0xcd,
  0x11,0x4f,0xad,0xf3,0x70,0x2e,0xcc,0x92,0xd3,0x8d,0x6f,0x31,0xb2,0xec,0x0e,0x50,
  0xaf,0xf1,0x13,0x4d,0xce,0x90,0x72,0x2c,0x6d,0x33,0xd1,0x8f,0x0c,0x52,0xb0,0xee,
  0x32,0x6c,0x8e,0xd0,0x53,0x0d,0xef,0xb1,0xf0,0xae,0x4c,0x12,0x91,0xcf,0x2d,0x73,
  0xca,0x94,0x76,0x28,0xab,0xf5,0x17,0x49,0x08,0x56,0xb4,0xea,0x69,0x37,0xd5,0x8b,
  0x57,0x09,0xeb,0xb5,0x36,0x68,0x8a,0xd4,0x95,0xcb,0x29,0x77,0xf4,0xaa,0x48,0x16,
  0xe9,0xb7,0x55,0x0b,0x88,0xd6,0x34,0x6a,0x2b,0x75,0x97,0xc9,0x4a,0x14,0xf6,0xa8,
  0x74,0x2a,0xc8,0x96,0x15,0x4b,0xa9,0xf7,0xb6,0xe8,0x0a,0x54,0xd7,0x89,0x6b,0x35,
};

inline uint8_t calc(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) crc = TABLE[crc ^ data[i]];
    return crc;
}

} // namespace gimbal_crc

// ========== 发送协议包（11字节） ==========
struct __attribute__((packed)) GimbalPacket {
    uint8_t head;
    float   pitch;
    float   yaw;
    uint8_t target_found;
    uint8_t crc8;
    uint8_t tail;
};

// ========== 串口通信类 ==========
class GimbalSerial
{
public:
    GimbalSerial(const std::string& port, int baud)
        : fd_(-1), port_(port), rx_len_(0),
          last_rx_yaw_(0.0f), rx_valid_(false)
    {
        fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) return;
        configure(baud);
    }

    ~GimbalSerial() {
        if (fd_ >= 0) close(fd_);
    }

    GimbalSerial(const GimbalSerial&)            = delete;
    GimbalSerial& operator=(const GimbalSerial&) = delete;

    bool is_open() const { return fd_ >= 0; }
    const std::string& port() const { return port_; }

    // ---- 发送 ----
    ssize_t send(float pitch, float yaw, uint8_t target_found)
    {
        if (fd_ < 0) return -1;

        GimbalPacket pkt;
        pkt.head         = 0x6A;
        pkt.pitch        = pitch;
        pkt.yaw          = yaw;
        pkt.target_found = target_found;
        pkt.crc8 = gimbal_crc::calc(
            reinterpret_cast<const uint8_t*>(&pkt.pitch), 9);
        pkt.tail = 0xA6;

        ssize_t n = write(fd_, &pkt, sizeof(pkt));
        tcdrain(fd_);
        return n;
    }

    ssize_t sendIdle() { return send(0.0f, 0.0f, 0); }

    // ---- 接收：非阻塞 ----
    void tryReceive()
    {
        if (fd_ < 0) return;

        uint8_t tmp[64];
        ssize_t n = read(fd_, tmp, sizeof(tmp));
        if (n <= 0) return;

        for (ssize_t i = 0; i < n; ++i) {
            rx_buf_[rx_len_++] = tmp[i];

            if (rx_len_ >= sizeof(rx_buf_)) {
                rx_len_ = 0;
            }

            if (rx_len_ >= RX_PKT_SIZE) {
                parseRxBuffer();
            }
        }
    }

    bool  hasRxData() const { return rx_valid_; }
    // 取走一帧，取完清标志：下次没新帧时 hasRxData() 返回 false
    float takeRxYaw() { rx_valid_ = false; return last_rx_yaw_; }

private:
    // 接收帧：[0x6A] [yaw 4B] [0xA6] = 6字节
    static constexpr size_t RX_PKT_SIZE = 6;

    void parseRxBuffer()
    {
        while (rx_len_ >= RX_PKT_SIZE) {
            if (rx_buf_[0] != 0x6A) {
                memmove(rx_buf_, rx_buf_ + 1, --rx_len_);
                continue;
            }

            if (rx_buf_[RX_PKT_SIZE - 1] != 0xA6) {
                memmove(rx_buf_, rx_buf_ + 1, --rx_len_);
                continue;
            }

            float yaw;
            memcpy(&yaw, &rx_buf_[1], 4);
            last_rx_yaw_ = yaw;
            rx_valid_ = true;

            if (rx_len_ > RX_PKT_SIZE) {
                memmove(rx_buf_, rx_buf_ + RX_PKT_SIZE, rx_len_ - RX_PKT_SIZE);
            }
            rx_len_ -= RX_PKT_SIZE;
        }
    }

    void configure(int baud)
    {
        struct termios tty{};
        tcgetattr(fd_, &tty);

        speed_t speed;
        switch (baud) {
            case 9600:   speed = B9600;   break;
            case 460800: speed = B460800; break;
            case 921600: speed = B921600; break;
            default:     speed = B115200; break;
        }
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_iflag = 0;
        tty.c_oflag = 0;
        tty.c_lflag = 0;
        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 0;

        tcsetattr(fd_, TCSANOW, &tty);
        tcflush(fd_, TCIOFLUSH);
    }

    int         fd_;
    std::string port_;

    uint8_t rx_buf_[32];
    size_t  rx_len_;
    float   last_rx_yaw_;
    bool    rx_valid_;
};