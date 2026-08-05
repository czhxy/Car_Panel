#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UART 环路验证脚本 — 验证显示域 mod_comm_uart 框架的正常收发

框架数据流（与 CAN 对齐）:
  RX: USART1_IRQHandler → Mod_Uart_RxIRQHandler → 字节队列 → UART_RX_Task 拼包 → 业务回调
  TX: Mod_Uart_SendPacket → TX 队列 → UART_TX_Task → UART_SendArray

验证内容:
  1. chip info 查询:   发 [AA 55 01 00]，MCU 应答 [AA 55 01 0D <13B 数据> <crc16>]
  2. 环路回环 echo:    发 [AA 55 10 04 01 02 03 04 <crc16>]，MCU 原样回发（验证 TX/RX 全链路）

用法:
  python uart_loopback_test.py <COM端口> [波特率=115200]
  例:  python uart_loopback_test.py COM3 115200

依赖: pyserial (pip install pyserial)
"""

import sys
import time

try:
    import serial
except ImportError:
    print("缺少 pyserial 依赖，请先执行: pip install pyserial")
    sys.exit(1)

HEADER = bytes([0xAA, 0x55])


def crc16(data: bytes) -> int:
    """YMODEM CRC16: poly 0x1021, init 0x0000, MSB first（与 MCU mod_comm_uart.c 一致）"""
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def build_frame(pkt_type: int, data: bytes) -> bytes:
    """按 MCU 帧格式组包: [AA 55 type len data crc16_hi crc16_lo]"""
    c = crc16(data)
    return HEADER + bytes([pkt_type, len(data)]) + data + bytes([(c >> 8) & 0xFF, c & 0xFF])


def recv_frame(ser: serial.Serial, timeout: float = 1.0):
    """读取一帧 [AA 55 type len data crc16]，返回 (type, data, ok, desc)"""
    start = time.time()

    # 找同步头 AA 55（跳过 MCU 的 printf 日志等非协议内容）
    while time.time() - start < timeout:
        if ser.in_waiting >= 2:
            b1 = ser.read(1)
            if b1 and b1[0] == 0xAA:
                b2 = ser.read(1)
                if b2 and b2[0] == 0x55:
                    break
        else:
            time.sleep(0.005)
    else:
        return (None, None, False, "超时：未收到帧头 AA 55")

    # type + len
    hdr = ser.read(2)
    if len(hdr) < 2:
        return (None, None, False, "帧头不完整")
    pkt_type, pkt_len = hdr[0], hdr[1]

    # data + crc
    rest = ser.read(pkt_len + 2)
    if len(rest) < pkt_len + 2:
        return (None, None, False, "帧体不完整")
    data = rest[:pkt_len]
    crc_recv = (rest[pkt_len] << 8) | rest[pkt_len + 1]
    crc_calc = crc16(data)
    if crc_recv != crc_calc:
        return (pkt_type, data, False,
                f"CRC 校验失败 recv=0x{crc_recv:04X} calc=0x{crc_calc:04X}")
    return (pkt_type, data, True, "OK")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    ser = serial.Serial(port, baud, timeout=0.1)
    time.sleep(0.2)
    ser.reset_input_buffer()

    passed = 0
    failed = 0

    def report(name, ok, desc):
        nonlocal passed, failed
        if ok:
            passed += 1
            print(f"[PASS] {name}: {desc}")
        else:
            failed += 1
            print(f"[FAIL] {name}: {desc}")

    # ---- 测试 1: chip info 查询（兼容 PC 旧 4 字节查询指令，无 CRC） ----
    print("\n=== 测试 1: chip info 查询 [AA 55 01 00] ===")
    ser.reset_input_buffer()
    ser.write(bytes([0xAA, 0x55, 0x01, 0x00]))
    t, data, ok, desc = recv_frame(ser, timeout=2.0)
    if ok and t == 0x01:
        mcu = ("STM32F429" if data and data[0] == 0xF4
               else f"code={data[0]:02X}" if data else "?")
        part = {0: "未知", 1: "App A", 2: "App B"}.get(data[1], "?") if len(data) > 1 else "?"
        report("chip info 应答", True,
               f"type=0x01 len={len(data) if data else 0} mcu={mcu} 分区={part}")
    else:
        report("chip info 应答", False, desc or f"type={t}")

    # ---- 测试 2: 环路回环 echo（验证 TX 队列 + UART_TX_Task 发送） ----
    echo_data = bytes([0x01, 0x02, 0x03, 0x04])
    echo_frame = build_frame(0x10, echo_data)
    print(f"\n=== 测试 2: 环路回环 {echo_frame.hex().upper()} ===")
    ser.reset_input_buffer()
    ser.write(echo_frame)
    t, data, ok, desc = recv_frame(ser, timeout=2.0)
    if ok and t == 0x10 and bytes(data) == echo_data:
        report("环路回环", True,
               f"type=0x10 数据 {bytes(data).hex().upper()} 与发送一致")
    else:
        report("环路回环", False,
               desc or f"type={t} data={bytes(data or b'').hex().upper()}")

    # ---- 汇总 ----
    print(f"\n=== 结果: {passed} PASS / {failed} FAIL ===")
    ser.close()
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
