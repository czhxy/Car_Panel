#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
OTA Core — YMODEM-1K 发送器 + 芯片信息查询
从 ymodem_send.py 改造而来，增加回调机制和取消支持。

用法 (直接被 main.py 调用):
    from ota_core import OTASender, ChipInfo
    sender = OTASender(log_callback, progress_callback, cancel_event)
    success = sender.ymodem_send(ser, filepath)
"""

import os
import time
import threading
from typing import Callable, Optional

# ======================== YMODEM 常量 ========================
SOH       = 0x01   # 128 字节包头
STX       = 0x02   # 1024 字节包头 (YMODEM-1K)
EOT       = 0x04   # 传输结束
ACK       = 0x06   # 确认
NAK       = 0x15   # 重传请求
CAN       = 0x18   # 取消传输
C_CHAR    = 0x43   # 'C'

PACKET_1K  = 1024   # YMODEM-1K 数据大小
PACKET_128 = 128    # 文件名包用 128 字节

CRC_POLY  = 0x1021

# ======================== 芯片信息查询指令 ========================
CHIP_QUERY_CMD = bytes([0xAA, 0x55, 0x01, 0x00])  # 查询指令


class OTASender:
    """YMODEM-1K 发送器，支持回调 + 取消"""

    def __init__(self,
                 log_cb: Optional[Callable[[str], None]] = None,
                 progress_cb: Optional[Callable[[int, int], None]] = None,
                 cancel_flag: Optional[threading.Event] = None):
        """
        初始化发送器。

        Args:
            log_cb: 日志回调，参数 (msg: str)
            progress_cb: 进度回调，参数 (total_sent: int, filesize: int)
            cancel_flag: 取消标志 (threading.Event)
        """
        self.log_cb = log_cb or (lambda msg: None)
        self.progress_cb = progress_cb or (lambda sent, total: None)
        self.cancel_flag = cancel_flag or threading.Event()
        self._boot_log = []  # 收集 bootloader 启动日志

    # ======================== CRC16 计算 ========================

    def _crc16(self, data: bytes) -> int:
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

    def _log(self, msg: str):
        """线程安全日志"""
        self.log_cb(msg)

    def _is_cancelled(self) -> bool:
        return self.cancel_flag.is_set()

    def _wait_for_byte(self, ser, expected: int, timeout_s: float = 60.0,
                        label: str = "ACK") -> bool:
        """等待特定字节，超时返回 False，支持取消"""
        start = time.time()
        while time.time() - start < timeout_s:
            if self._is_cancelled():
                self._log(f"  [取消] 等待 {label} 时取消")
                return False
            if ser.in_waiting:
                ch = ser.read(1)
                if not ch:
                    continue
                if ch[0] == expected:
                    return True
                if ch[0] == NAK:
                    self._log(f"  [WAIT] 收到 NAK 而非 {label}")
                    return False
                if ch[0] == CAN:
                    self._log(f"  [WAIT] 收到 CAN (传输取消)")
                    return False
                # 收集非预期字符（可能是 MCU 调试输出）
                try:
                    ch_str = ch.decode('ascii', errors='replace')
                    self._log(f"  [MCU] {ch_str.rstrip()}")
                except Exception:
                    pass
            time.sleep(0.01)
        self._log(f"  [WAIT] 等待 {label} 超时 ({timeout_s}s)")
        return False

    def _wait_for_ack_or_c(self, ser, timeout_s: float = 60.0) -> bool:
        """等待 ACK 或 'C' (第二个 seq=0 包的应答)，支持取消"""
        start = time.time()
        while time.time() - start < timeout_s:
            if self._is_cancelled():
                self._log("  [取消] 等待 ACK/C 时取消")
                return False
            if ser.in_waiting:
                ch = ser.read(1)
                if not ch:
                    continue
                if ch[0] in (ACK, C_CHAR):
                    return True
                if ch[0] == NAK:
                    self._log("  [WAIT] 收到 NAK")
                    return False
                if ch[0] == CAN:
                    self._log("  [WAIT] 收到 CAN (传输取消)")
                    return False
            time.sleep(0.01)
        self._log(f"  [WAIT] 等待 ACK/C 超时 ({timeout_s}s)")
        return False

    def _wait_for_c(self, ser, timeout_s: float = 120.0) -> bool:
        """等待 MCU 发送 'C' (轮询请求)，同时收集 bootloader 启动日志"""
        self._log("等待 MCU 发送 'C'...")
        self._boot_log = []  # 重置
        start = time.time()
        last_print = 0
        while time.time() - start < timeout_s:
            if self._is_cancelled():
                self._log("  [取消] 等待 'C' 时取消")
                return False
            if ser.in_waiting:
                ch = ser.read(1)
                if not ch:
                    continue
                if ch[0] == C_CHAR:
                    self._log("收到 'C', 开始传输.")
                    return True
                # 回显 MCU 的调试输出，同时收集到 boot_log
                try:
                    text = ch.decode('ascii', errors='replace')
                    self._boot_log.append(text)
                    self._log(f"  [MCU] {text.rstrip()}")
                except Exception:
                    pass
            # 每 5 秒打印一次状态
            now = time.time()
            if now - last_print > 5:
                self._log("  仍在等待 'C'...")
                last_print = now
            time.sleep(0.01)
        self._log("等待 'C' 超时!")
        return False

    def get_boot_log_text(self) -> str:
        """获取收集的 bootloader 启动日志 (用于芯片信息解析)"""
        return ''.join(self._boot_log)

    # ======================== 数据包发送 ========================

    def _send_packet(self, ser, seq: int, data: bytes):
        """发送一个 YMODEM-1K 数据包 (STX 头)"""
        seq_byte = seq & 0xFF
        seq_inv = (~seq) & 0xFF

        pkt = bytearray()
        pkt.append(STX)          # 头: 1024 字节包
        pkt.append(seq_byte)     # 序号
        pkt.append(seq_inv)      # 序号反码
        pkt.extend(data)         # 1024 字节数据

        crc = self._crc16(data)
        pkt.append((crc >> 8) & 0xFF)   # CRC16 高字节
        pkt.append(crc & 0xFF)          # CRC16 低字节

        ser.write(bytes(pkt))
        ser.flush()

    def _send_filename_packet(self, ser, seq: int, data: bytes):
        """发送文件名包 (SOH 头, 128 字节)"""
        seq_byte = seq & 0xFF
        seq_inv = (~seq) & 0xFF

        pkt = bytearray()
        pkt.append(SOH)          # 头: 128 字节包
        pkt.append(seq_byte)     # 序号
        pkt.append(seq_inv)      # 序号反码
        pkt.extend(data)         # 128 字节数据

        crc = self._crc16(data)
        pkt.append((crc >> 8) & 0xFF)
        pkt.append(crc & 0xFF)

        ser.write(bytes(pkt))
        ser.flush()

    # ======================== 主发送流程 ========================

    def ymodem_send(self, ser, filepath: str) -> bool:
        """执行完整的 YMODEM-1K 发送流程，返回成功/失败"""
        if not os.path.exists(filepath):
            self._log(f"错误: 文件不存在 — {filepath}")
            return False

        filename = os.path.basename(filepath)
        filesize = os.path.getsize(filepath)
        mtime = os.path.getmtime(filepath)
        age = time.time() - mtime
        mtime_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(mtime))

        self._log(f"文件: {filename}, {filesize} 字节 ({mtime_str})")
        if age > 300:
            self._log(f"*** 警告: 文件 {age / 60:.0f} 分钟前生成，可能是旧版本！")

        self._log("=" * 50)
        self._log(f"  YMODEM-1K 发送工具")
        self._log(f"  文件: {filename}")
        self._log(f"  大小: {filesize} 字节 ({filesize / 1024:.1f} KB)")
        self._log("=" * 50)

        # ===== Step 1: 等待 MCU 发送 'C' =====
        if not self._wait_for_c(ser):
            return False

        # ===== Step 2: 发送文件名包 (序号 0, 128 字节) =====
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
        offset = len(name_bytes) + 1
        for i, b in enumerate(size_str):
            if offset + i < PACKET_128:
                pkt[offset + i] = b

        self._log(f"发送文件名包 (seq=0) -> {filename} / {filesize} 字节")
        self._send_filename_packet(ser, 0, bytes(pkt))

        if not self._wait_for_byte(ser, ACK, label="ACK (文件名包应答)"):
            return False
        self._log("  文件名包 ACK 收到")

        # ===== Step 3: 等待 MCU 擦除完成, 发 'C' 请求数据 =====
        self._log("等待 MCU 擦除并请求数据...")
        if not self._wait_for_byte(ser, C_CHAR, timeout_s=30.0, label="'C' (数据请求)"):
            return False
        self._log("  收到 'C', 开始发送数据")

        # ===== Step 4: 发送数据包 =====
        self._log("开始发送数据...")
        seq = 1
        total_sent = 0

        with open(filepath, 'rb') as f:
            while True:
                if self._is_cancelled():
                    self._log("用户取消传输!")
                    return False

                data = f.read(PACKET_1K)
                if not data:
                    break

                # 最后不满 1024 字节, 用 0x00 填充
                if len(data) < PACKET_1K:
                    data = data + b'\x00' * (PACKET_1K - len(data))

                self._send_packet(ser, seq, data)

                if not self._wait_for_byte(ser, ACK, timeout_s=10.0,
                                           label=f"ACK (seq={seq})"):
                    return False

                total_sent += len(data)
                # 回调进度
                self.progress_cb(min(total_sent, filesize), filesize)

                pct = total_sent * 100 // filesize if filesize else 100
                self._log(f"  包 seq={seq:3d} | "
                          f"{min(total_sent, filesize)}/{filesize} ({pct}%)")

                seq = (seq + 1) & 0xFF

        self._log(f"数据发送完毕, 共 {total_sent} 字节")

        # ===== Step 5: 发送 EOT (结束传输) =====
        if self._is_cancelled():
            self._log("用户取消传输!")
            return False

        self._log("发送 EOT...")
        ser.write(bytes([EOT]))
        ser.flush()

        if not self._wait_for_byte(ser, NAK, timeout_s=10.0, label="NAK (第一次)"):
            return False
        self._log("  收到 NAK, 发送第二次 EOT...")

        ser.write(bytes([EOT]))
        ser.flush()

        if not self._wait_for_byte(ser, ACK, timeout_s=10.0, label="ACK (EOT 应答)"):
            return False
        self._log("  EOT 应答 OK")

        # ===== Step 6: 发送空文件名包 (结束传输) =====
        self._log("发送结束标志包...")
        self._send_filename_packet(ser, 0, b'\x00' * PACKET_128)

        if not self._wait_for_byte(ser, ACK, timeout_s=10.0, label="ACK (结束包)"):
            return False

        self._log("传输完成!")
        return True


class ChipInfo:
    """芯片信息查询 — 两种方式获取 MCU 信息"""

    # 查询指令常量
    QUERY_CMD = bytes([0xAA, 0x55, 0x01, 0x00])

    @staticmethod
    def parse_boot_log(text: str) -> dict:
        """
        方式一：从 bootloader 启动日志中解析芯片信息。

        解析常见的 bootloader/App 输出格式:
          Bootloader v1.0 (STM32F429)
          App Software Version: v1.0
          Active partition: App A
          [BOOT] App A ver: 0x00010002, size: 262144

        以及 App 启动输出:
          App v1.0 (STM32F429) @ 0x08020000

        Args:
            text: bootloader/app 启动日志纯文本

        Returns:
            dict: 解析出的键值对，未识别字段值为 "未知"
        """
        import re
        info = {
            "mcu": "未知",
            "active_partition": "未知",
            "boot_addr": "未知",
            "app_addr": "未知",
            "version": "未知",
            "firmware_size": "未知",
            "crc32": "未知",
        }

        # MCU 型号 — 匹配 STM32Fxxx
        m = re.search(r'STM32F(\d+)', text, re.IGNORECASE)
        if m:
            info["mcu"] = f"STM32F{m.group(1)}"

        # 活跃分区 — 中英文双匹配
        m = re.search(r'活跃分区[:：]\s*(.+)', text)
        if not m:
            m = re.search(r'Active partition[:：]\s*(.+)', text, re.IGNORECASE)
        if m:
            part = m.group(1).strip()
            # 规范化: "App A" / "App B" or 0 / 1
            if part in ("0", "App A"):
                info["active_partition"] = "App A"
            elif part in ("1", "App B"):
                info["active_partition"] = "App B"
            else:
                info["active_partition"] = part
        elif re.search(r'App\s*A', text):
            info["active_partition"] = "App A"
        elif re.search(r'App\s*B', text):
            info["active_partition"] = "App B"

        # 软件版本号 — 匹配多种格式
        # 格式1: "App Software Version: v1.0" (bootloader)
        # 格式2: "App v1.0 (STM32F429)" (App 启动)
        m = re.search(r'App (?:Software )?Version[:：]\s*v?([\d.]+)', text, re.IGNORECASE)
        if not m:
            m = re.search(r'App\s+v?([\d.]+)\s*\(', text)
        if m:
            info["version"] = f"v{m.group(1)}"

        # BOOT 地址 — 从 Flash 行解析
        m = re.search(r'(?:BOOT[:：]\s*(0x[0-9A-Fa-f]+)|Flash:\s*\d+KB\s*\((0x[0-9A-Fa-f]+))', text)
        if m:
            info["boot_addr"] = m.group(1) or m.group(2)

        # APP 地址 — 从 @ 0x... 解析
        m = re.search(r'@\s*(0x[0-9A-Fa-f]+)', text)
        if m:
            info["app_addr"] = m.group(1)

        # OTA 参数中的固件版本 (App A ver / App B ver)
        if info["active_partition"] == "App A":
            m = re.search(r'App A ver[:：]\s*(\S+)', text)
        elif info["active_partition"] == "App B":
            m = re.search(r'App B ver[:：]\s*(\S+)', text)
        else:
            m = re.search(r'App [AB] ver[:：]\s*(\S+)', text)
        if m and info["version"] == "未知":
            # 如果没有软件版本号，回退用 OTA param version
            info["version"] = m.group(1)

        # 固件大小
        m = re.search(r'固件大小[:：]\s*(\d+)', text)
        if not m:
            # 从 OTA param 中解析
            if info["active_partition"] == "App A":
                m = re.search(r'App A ver:.*?size:\s*(\d+)', text)
            elif info["active_partition"] == "App B":
                m = re.search(r'App B ver:.*?size:\s*(\d+)', text)
            else:
                m = re.search(r'size:\s*(\d+)', text)
        if m:
            info["firmware_size"] = m.group(1)

        # CRC32
        m = re.search(r'CRC(?:32)?[:：]\s*(0x[0-9A-Fa-f]+)', text)
        if m:
            info["crc32"] = m.group(1)

        return info

    @staticmethod
    def query_by_command(ser,
                         log_cb: Optional[Callable[[str], None]] = None,
                         timeout_ms: int = 2000,
                         cancel_flag=None) -> dict:
        """
        方式二：通过串口查询指令获取芯片信息 (需 MCU 固件配合)。

        PC 发送 0xAA 0x55 0x01 0x00 查询指令，MCU 返回结构化数据包。

        返回的数据包格式:
          [0xAA, 0x55, type, len, data..., crc16]

        Args:
            ser: 已打开的串口对象
            log_cb: 日志回调
            timeout_ms: 超时毫秒数
            cancel_flag: threading.Event，用于取消操作

        Returns:
            dict: 芯片信息字典
        """
        log = log_cb or (lambda msg: None)
        info = {
            "mcu": "未知",
            "active_partition": "未知",
            "boot_addr": "未知",
            "app_addr": "未知",
            "version": "未知",
            "firmware_size": "未知",
            "crc32": "未知",
        }

        try:
            # 清空缓冲区
            ser.reset_input_buffer()

            # 发送查询指令
            ser.write(ChipInfo.QUERY_CMD)
            ser.flush()
            log("发送芯片信息查询指令...")

            # 等待响应包头 0xAA 0x55
            start = time.time()
            timeout_s = timeout_ms / 1000.0

            # 找同步头
            while time.time() - start < timeout_s:
                if cancel_flag and cancel_flag.is_set():
                    log("查询已取消")
                    return info
                if ser.in_waiting >= 2:
                    b1 = ser.read(1)
                    if b1 and b1[0] == 0xAA:
                        b2 = ser.read(1)
                        if b2 and b2[0] == 0x55:
                            break
                    else:
                        continue
                time.sleep(0.01)
            else:
                log("超时: 未收到芯片信息响应包头")
                return info

            # 读取类型和长度
            if ser.in_waiting < 2:
                # 等待数据到达
                time.sleep(0.05)
            if ser.in_waiting < 2:
                log("响应包数据不足")
                return info

            pkt_type = ser.read(1)[0]
            pkt_len = ser.read(1)[0]

            # 等待数据部分
            timeout_data = time.time()
            while ser.in_waiting < (pkt_len + 2) and (time.time() - timeout_data) < 1.0:
                if cancel_flag and cancel_flag.is_set():
                    log("查询已取消")
                    return info
                time.sleep(0.01)

            if ser.in_waiting < pkt_len + 2:
                log("数据包不完整")
                return info

            # 读取数据
            data = ser.read(pkt_len)

            # 读取 CRC
            crc_bytes = ser.read(2)
            # crc_received = (crc_bytes[0] << 8) | crc_bytes[1]  # 可添加 CRC 校验

            # 解析数据字段
            # MCU 型号 (0xF4 = STM32F429, 其余按 STM32F{code:03d})
            if pkt_len >= 4:
                mcu_code = data[0]
                if mcu_code == 0xF4:
                    info["mcu"] = "STM32F429"
                elif mcu_code == 0xF7:
                    info["mcu"] = "STM32F7"
                elif mcu_code == 0x41:
                    info["mcu"] = "STM32F4"
                elif mcu_code:
                    info["mcu"] = f"STM32F{mcu_code:03d}"

            if pkt_len >= 5:
                partitions = {0: "未知", 1: "App A", 2: "App B"}
                info["active_partition"] = partitions.get(data[1], "未知")

            if pkt_len >= 9:
                boot_addr = int.from_bytes(data[2:6], 'little')
                info["boot_addr"] = f"0x{boot_addr:08X}"

            if pkt_len >= 13:
                app_addr = int.from_bytes(data[6:10], 'little')
                info["app_addr"] = f"0x{app_addr:08X}"
                info["version"] = f"v{data[10]}.{data[11]}.{data[12]}"

            log("芯片信息查询成功")
            return info

        except Exception as e:
            log(f"查询芯片信息异常: {e}")
            return info
