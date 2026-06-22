#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
YMODEM-1K Sender — 用于 STM32F411 OTA Bootloader
用法: python ymodem_send.py <COM端口> <BIN文件>
示例: python ymodem_send.py COM3 app.bin

依赖: pip install pyserial

协议流程 (与 MCU ymodem.c 对应):
  MCU 轮询发 'C' → PC 发文件名包(seq=0) → MCU ACK
  → PC 发第二个 seq=0 包 → MCU 发 'C'
  → 数据包 seq=1..N → MCU 每包 ACK
  → PC 发 EOT → MCU NAK → PC 第二次 EOT → MCU ACK
  → PC 发空文件名包 → MCU ACK → 完成
"""

import serial
import sys
import os
import time
import struct
import argparse

# ======================== YMODEM 常量 ========================
SOH       = 0x01   # 128 字节包头
STX       = 0x02   # 1024 字节包头 (YMODEM-1K)
EOT       = 0x04   # 传输结束
ACK       = 0x06   # 确认
NAK       = 0x15   # 重传请求
CAN       = 0x18   # 取消传输
C_CHAR    = 0x43   # 'C'

PACKET_1K = 1024   # YMODEM-1K 数据大小
PACKET_128 = 128   # 文件名包用 128 字节

CRC_POLY  = 0x1021

# ======================== CRC16 计算 ========================

def crc16(data: bytes) -> int:
    """YMODEM CRC16 (多项式 0x1021, 初始值 0x0000, MSB first)"""
    crc = 0
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ CRC_POLY
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc


# ======================== 串口辅助 ========================

def wait_for_byte(ser, expected: int, timeout_s: float = 60.0,
                  label: str = "ACK") -> bool:
    """等待特定字节, 超时返回 False"""
    start = time.time()
    while time.time() - start < timeout_s:
        if ser.in_waiting:
            ch = ser.read(1)
            if not ch:
                continue
            if ch[0] == expected:
                return True
            if ch[0] == NAK:
                print(f"  [WAIT] 收到 NAK 而非 {label}")
                return False
            if ch[0] == CAN:
                print(f"  [WAIT] 收到 CAN (传输取消)")
                return False
        time.sleep(0.01)
    print(f"  [WAIT] 等待 {label} 超时 ({timeout_s}s)")
    return False


def wait_for_ack_or_c(ser, timeout_s: float = 60.0) -> bool:
    """等待 ACK 或 'C' (第二个 seq=0 包的应答)"""
    start = time.time()
    while time.time() - start < timeout_s:
        if ser.in_waiting:
            ch = ser.read(1)
            if not ch:
                continue
            if ch[0] in (ACK, C_CHAR):
                return True
            if ch[0] == NAK:
                print("  [WAIT] 收到 NAK")
                return False
            if ch[0] == CAN:
                print("  [WAIT] 收到 CAN (传输取消)")
                return False
        time.sleep(0.01)
    print(f"  [WAIT] 等待 ACK/C 超时 ({timeout_s}s)")
    return False


def wait_for_c(ser, timeout_s: float = 120.0) -> bool:
    """等待 MCU 发送 'C' (轮询请求)"""
    print("[SEND] 等待 MCU 发送 'C'...")
    start = time.time()
    last_print = 0
    while time.time() - start < timeout_s:
        if ser.in_waiting:
            ch = ser.read(1)
            if not ch:
                continue
            if ch[0] == C_CHAR:
                print("[SEND] 收到 'C', 开始传输.")
                return True
            # 回显 MCU 的调试输出
            try:
                sys.stdout.write(ch.decode('ascii', errors='replace'))
                sys.stdout.flush()
            except Exception:
                pass
        # 每 5 秒打印一次状态
        now = time.time()
        if now - last_print > 5:
            print("  [SEND] 仍在等待 'C'...")
            last_print = now
        time.sleep(0.01)
    print("[SEND] 等待 'C' 超时!")
    return False


# ======================== 数据包发送 ========================

def send_packet(ser, seq: int, data: bytes):
    """发送一个 YMODEM-1K 数据包 (STX 头)"""
    seq_byte = seq & 0xFF
    seq_inv = (~seq) & 0xFF

    pkt = bytearray()
    pkt.append(STX)          # 头: 1024 字节包
    pkt.append(seq_byte)     # 序号
    pkt.append(seq_inv)      # 序号反码
    pkt.extend(data)         # 1024 字节数据

    crc = crc16(data)
    pkt.append((crc >> 8) & 0xFF)   # CRC16 高字节
    pkt.append(crc & 0xFF)          # CRC16 低字节

    ser.write(bytes(pkt))
    ser.flush()


def send_filename_packet(ser, seq: int, data: bytes):
    """发送文件名包 (SOH 头, 128 字节)"""
    seq_byte = seq & 0xFF
    seq_inv = (~seq) & 0xFF

    pkt = bytearray()
    pkt.append(SOH)          # 头: 128 字节包
    pkt.append(seq_byte)     # 序号
    pkt.append(seq_inv)      # 序号反码
    pkt.extend(data)         # 128 字节数据

    crc = crc16(data)
    pkt.append((crc >> 8) & 0xFF)
    pkt.append(crc & 0xFF)

    ser.write(bytes(pkt))
    ser.flush()


# ======================== 主发送流程 ========================

def ymodem_send(ser, filepath: str) -> bool:
    """执行完整的 YMODEM-1K 发送流程"""
    if not os.path.exists(filepath):
        print(f"[SEND] 错误: 文件不存在 — {filepath}")
        return False

    filename = os.path.basename(filepath)
    filesize = os.path.getsize(filepath)
    mtime = os.path.getmtime(filepath)
    age = time.time() - mtime
    mtime_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(mtime))
    print(f"[SEND] 文件: {filename}, {filesize} 字节"
          f" ({mtime_str})")
    if age > 300:
        print(f"[SEND] *** 警告: 文件 {age/60:.0f} 分钟前生成，可能是旧版本！")

    print("=" * 50)
    print(f"  YMODEM-1K 发送工具")
    print(f"  文件: {filename}")
    print(f"  大小: {filesize} 字节 ({filesize / 1024:.1f} KB)")
    print("=" * 50)
    print()

    # ===== Step 1: 等待 MCU 发送 'C' =====
    if not wait_for_c(ser):
        return False

    # ===== Step 2: 发送文件名包 (序号 0, 128 字节) =====
    # 格式: 文件名(NUL 结尾) + 文件大小(ASCII, 空格/NUL 结尾) + 补 NUL
    pkt = bytearray(PACKET_128)
    for i in range(PACKET_128):
        pkt[i] = 0x00

    # 文件名
    name_bytes = filename.encode('ascii')
    for i, b in enumerate(name_bytes):
        if i < (PACKET_128 - 16):  # 留 16 字节给大小
            pkt[i] = b

    # 文件大小 (ASCII)
    size_str = str(filesize).encode('ascii')
    offset = len(name_bytes) + 1  # 跳过文件名 + NUL
    for i, b in enumerate(size_str):
        if offset + i < PACKET_128:
            pkt[offset + i] = b

    print(f"[SEND] 发送文件名包 (seq=0) -> {filename} / {filesize} 字节")
    send_filename_packet(ser, 0, bytes(pkt))

    # 等待 ACK
    if not wait_for_byte(ser, ACK, label="ACK (文件名包应答)"):
        return False
    print("  [SEND] 文件名包 ACK 收到")

    # ===== Step 3: 等待 MCU 擦除完成, 发 'C' 请求数据 =====
    print("[SEND] 等待 MCU 擦除并请求数据...")
    if not wait_for_byte(ser, C_CHAR, timeout_s=30.0, label="'C' (数据请求)"):
        return False
    print("  [SEND] 收到 'C', 开始发送数据")

    # ===== Step 4: 发送数据包 =====
    print(f"[SEND] 开始发送数据...")
    seq = 1
    total_sent = 0

    with open(filepath, 'rb') as f:
        while True:
            data = f.read(PACKET_1K)
            if not data:
                break

            # 最后不满 1024 字节, 用 0x00 填充
            if len(data) < PACKET_1K:
                data = data + b'\x00' * (PACKET_1K - len(data))

            send_packet(ser, seq, data)

            if not wait_for_byte(ser, ACK, timeout_s=10.0,
                                 label=f"ACK (seq={seq})"):
                return False

            total_sent += len(data)
            pct = total_sent * 100 // filesize if filesize else 100
            # 每包都打印进度 (包数不多时)
            print(f"  [SEND] 包 seq={seq:3d} | "
                  f"{min(total_sent, filesize)}/{filesize} ({pct}%%)")

            seq = (seq + 1) & 0xFF

    print(f"[SEND] 数据发送完毕, 共 {total_sent} 字节")
    print(f"[SEND] 最后一个包号: seq={(seq - 1) & 0xFF}")

    # ===== Step 5: 发送 EOT (结束传输) =====
    print("[SEND] 发送 EOT...")
    ser.write(bytes([EOT]))
    ser.flush()

    # 等待 NAK
    if not wait_for_byte(ser, NAK, timeout_s=10.0, label="NAK (第一次)"):
        return False
    print("  [SEND] 收到 NAK, 发送第二次 EOT...")

    # 发送第二次 EOT
    ser.write(bytes([EOT]))
    ser.flush()

    # 等待 ACK
    if not wait_for_byte(ser, ACK, timeout_s=10.0, label="ACK (EOT 应答)"):
        return False
    print("  [SEND] EOT 应答 OK")

    # ===== Step 6: 发送空文件名包 (结束传输) =====
    print("[SEND] 发送结束标志包...")
    send_filename_packet(ser, 0, b'\x00' * PACKET_128)

    if not wait_for_byte(ser, ACK, timeout_s=10.0, label="ACK (结束包)"):
        return False

    print("\n[SEND] 传输完成!")
    return True


# ======================== 主入口 ========================

def main():
    parser = argparse.ArgumentParser(
        description="YMODEM-1K 固件发送工具 (STM32F411 OTA Bootloader)")
    parser.add_argument("port", help="串口 (如 COM3, /dev/ttyUSB0)")
    parser.add_argument("file", help="BIN 文件路径")
    parser.add_argument("-b", "--baudrate", type=int, default=115200,
                        help="波特率 (默认: 115200)")
    args = parser.parse_args()

    print(f"打开串口 {args.port} (波特率 {args.baudrate})...")
    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.5
        )
    except serial.SerialException as e:
        print(f"串口错误: {e}")
        sys.exit(1)

    # 复位 DTR (部分板子 DTR 会触发 MCU 复位)
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(0.3)

    # 清空遗留数据
    ser.reset_input_buffer()

    success = ymodem_send(ser, args.file)

    ser.close()

    print()
    if success:
        print("=" * 50)
        print("  成功: 固件已发送!")
        print("=" * 50)
    else:
        print("=" * 50)
        print("  失败: 传输中断!")
        print("=" * 50)
        sys.exit(1)


if __name__ == "__main__":
    main()
