#!/usr/bin/env python
# coding: utf-8
'''
海康工业相机 图像格式转换 Python 封装（替代原大华版本）
功能：将相机原始图像（如 Bayer、YUV）转换为 BGR24/RGB24/Mono8 等通用格式
依赖：海康 MVS SDK 的 libMvCameraControl.so 库
'''
from ctypes import *
import os

# -------------------------- 1. 加载海康相机核心动态库 --------------------------
# 直接指定海康 SDK 的绝对路径（替换为你的实际路径）
hik_sdk_64_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "MVS", "lib", "64")  # 这里填写你的SDK 64位目录绝对路径

# 验证路径是否存在（可选，但建议保留，方便排查错误）
if not os.path.exists(f"{hik_sdk_64_path}/libMvCameraControl.so"):
    raise FileNotFoundError(f"海康SDK库文件不存在！请检查路径是否正确：\n{hik_sdk_64_path}/libMvCameraControl.so")

# 加载海康核心库（含图像转换接口）
from ctypes import cdll
MvCamCtrldll = cdll.LoadLibrary(f"{hik_sdk_64_path}/libMvCameraControl.so")


# -------------------------- 2. 定义海康 SDK 枚举类型（错误码、像素格式、转换算法） --------------------------
def enum(**enums):
    return type('Enum', (), enums)

# 2.1 海康 SDK 错误码（对应 MV_CC_ERROR_CODE 枚举）
MV_CC_ErrCode = enum(
    MV_OK = 0,                     # 成功
    MV_E_HANDLE = 1,               # 句柄无效
    MV_E_PARAM = 2,                # 参数非法（如宽高为负）
    MV_E_MEMORY = 3,               # 内存分配失败
    MV_E_NOT_SUPPORT = 4,          # 不支持的功能（如格式无法转换）
    MV_E_DEVICE_NOT_OPEN = 5,      # 设备未打开
    MV_E_BUFFER_TOO_SMALL = 6      # 目标缓冲区不足
)

# 2.2 海康 像素格式枚举（对应 MV_PIXEL_FORMAT 枚举，常用格式）
MV_PixelFormat = enum(
    # 原始格式（相机输出）
    MV_PIXEL_FORMAT_BAYER_RG8 = 0x00000008,  # Bayer RG8（8位，常用）
    MV_PIXEL_FORMAT_BAYER_GR8 = 0x00000009,  # Bayer GR8
    MV_PIXEL_FORMAT_YUV422_8 = 0x00000010,   # YUV422（8位）
    # 目标格式（转换后通用格式）
    MV_PIXEL_FORMAT_BGR8 = 0x00000018,       # BGR24（3字节/像素，OpenCV 默认）
    MV_PIXEL_FORMAT_RGB8 = 0x00000019,       # RGB24（3字节/像素，PyQt/PIL 常用）
    MV_PIXEL_FORMAT_MONO8 = 0x00000007       # Mono8（单通道灰度，1字节/像素）
)

# 2.3 海康 Bayer 去马赛克算法（对应 MV_BAYER_CONVERT_ALGORITHM 枚举）
MV_BayerConvertAlg = enum(
    MV_BAYER_CONVERT_NEAREST = 0,    # 最近邻算法（速度快，画质一般）
    MV_BAYER_CONVERT_BILINEAR = 1,   # 双线性插值（画质中等，速度中等）
    MV_BAYER_CONVERT_EDGE = 2        # 边缘感知算法（画质最好，速度较慢）
)

# -------------------------- 3. 定义海康 图像参数结构体（转换输入输出） --------------------------
# 3.1 图像帧信息结构体（对应 MV_FRAME_OUT_INFO_EX，存储原始图像信息）
class MV_FRAME_OUT_INFO_EX(Structure):
    _fields_ = [
        ('nWidth', c_uint),          # 图像宽度（像素）
        ('nHeight', c_uint),         # 图像高度（像素）
        ('nPixelFormat', c_uint),    # 像素格式（对应 MV_PixelFormat 枚举）
        ('nFrameNum', c_ulonglong),  # 帧号
        ('nBufferSize', c_uint),     # 图像数据大小（字节）
        ('nTimeStamp', c_ulonglong), # 时间戳（微秒）
        ('enPixelType', c_int)       # 像素类型（兼容旧版本，一般用 nPixelFormat）
    ]

# 3.2 像素格式转换参数结构体（对应 MV_CC_PIXEL_CONVERT_PARAM，转换配置）
class MV_CC_PIXEL_CONVERT_PARAM(Structure):
    _fields_ = [
        ('pSrcData', c_char_p),              # 原始图像数据指针
        ('nSrcDataLen', c_uint),             # 原始图像数据长度（字节）
        ('stSrcFrameInfo', MV_FRAME_OUT_INFO_EX),  # 原始图像帧信息
        ('pDstBuffer', c_char_p),            # 目标图像缓冲区指针
        ('nDstBufferSize', c_uint),          # 目标图像缓冲区大小（字节）
        ('nDstPixelFormat', c_uint),         # 目标像素格式（对应 MV_PixelFormat 枚举）
        ('nBayerConvertAlg', c_int),         # Bayer 去马赛克算法（对应 MV_BayerConvertAlg 枚举）
        ('nReserved', c_uint * 4)            # 保留字段（填0即可）
    ]

# -------------------------- 4. 封装海康图像转换函数（核心功能） --------------------------
def MV_CC_ConvertPixelType(pSrcData, stSrcFrameInfo, nDstPixelFormat, nBayerAlg=MV_BayerConvertAlg.MV_BAYER_CONVERT_BILINEAR):
    '''
    海康图像格式转换函数（替代原大华的 IMGCNV_ConvertToXXX）
    参数：
        pSrcData: 原始图像数据（bytes 类型，如相机回调获取的帧数据）
        stSrcFrameInfo: 原始图像帧信息（MV_FRAME_OUT_INFO_EX 结构体，含宽高、原始格式）
        nDstPixelFormat: 目标像素格式（如 MV_PixelFormat.MV_PIXEL_FORMAT_BGR8）
        nBayerAlg: Bayer 去马赛克算法（默认双线性，原始格式非 Bayer 时无效）
    返回：
        (转换结果, 目标图像数据, 目标数据长度)：结果为 MV_CC_ErrCode 枚举，成功则返回 MV_OK
    '''
    # 1. 计算目标图像缓冲区大小（根据目标格式确定字节数/像素）
    if nDstPixelFormat in [MV_PixelFormat.MV_PIXEL_FORMAT_BGR8, MV_PixelFormat.MV_PIXEL_FORMAT_RGB8]:
        nDstDataLen = stSrcFrameInfo.nWidth * stSrcFrameInfo.nHeight * 3  # 3字节/像素
    elif nDstPixelFormat == MV_PixelFormat.MV_PIXEL_FORMAT_MONO8:
        nDstDataLen = stSrcFrameInfo.nWidth * stSrcFrameInfo.nHeight * 1  # 1字节/像素
    else:
        return (MV_CC_ErrCode.MV_E_NOT_SUPPORT, None, 0)  # 不支持的目标格式

    # 2. 分配目标缓冲区（ctypes 字符串缓冲区，可写）
    pDstBuffer = create_string_buffer(nDstDataLen)

    # 3. 初始化转换参数结构体
    stConvertParam = MV_CC_PIXEL_CONVERT_PARAM()
    stConvertParam.pSrcData = c_char_p(pSrcData)
    stConvertParam.nSrcDataLen = len(pSrcData)
    stConvertParam.stSrcFrameInfo = stSrcFrameInfo
    stConvertParam.pDstBuffer = pDstBuffer
    stConvertParam.nDstBufferSize = nDstDataLen
    stConvertParam.nDstPixelFormat = nDstPixelFormat
    stConvertParam.nBayerConvertAlg = nBayerAlg
    stConvertParam.nReserved = (c_uint * 4)(0, 0, 0, 0)  # 保留字段填0

    # 4. 调用海康 SDK 转换接口
    ret = MvCamCtrldll.MV_CC_ConvertPixelType(byref(stConvertParam))

    # 5. 返回结果：成功则返回目标数据，失败则返回 None
    if ret == MV_CC_ErrCode.MV_OK:
        return (ret, pDstBuffer.raw, nDstDataLen)
    else:
        return (ret, None, 0)