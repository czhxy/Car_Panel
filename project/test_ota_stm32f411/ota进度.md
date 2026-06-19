# OTA 实现进度

> 最后更新：2026-06-19
> 文件用途：AI 读取此文件即可了解当前进度、下一步任务和已知问题

---

## 总体进度

```
阶段一  [████████] 工程配置
阶段二  [████████] Bootloader 骨架
阶段三  [████████] Flash 操作层
阶段四  [████████] CRC32 校验
阶段五  [████████] OTA Parameter 管理
阶段六  [████████] YMODEM 协议实现
阶段七  [████████] 升级状态机与回滚
阶段八  [████████] App 侧工程配置
阶段九  [████████] PC 端发送工具
阶段十  [████████] 联调与验证                  ← 已完成

已完成：10/10
```

---

## 已完成详情

### 阶段一：工程配置 ✅

- **`boot_config.h`** — 分区地址宏定义（48KB Bootloader / 16KB Parameter / 192KB App A / 256KB App B）
- **`mdk/bootloader.sct`** — Bootloader 分散加载（0x08000000, 48KB）
- **`mdk/app.sct`** — App A 分散加载（0x08010000, 192KB）
- **`mdk/app.uvprojx`** — App 工程（Define: `STM32F411xE,...`，无 BOOTLOADER 宏）
- **`mdk/bootloader.uvprojx`** — Bootloader 工程（Define: 含 `BOOTLOADER` 宏）
- **`mdk/workspace.uvmpw`** — 多工程工作区
- **`system_stm32f4xx.c`** — VECT_TAB_OFFSET 按 BOOTLOADER 宏区分，HSE 25MHz 支持

### 阶段二：Bootloader 骨架 ✅

- **`bootloader/boot_main.c`** — `jump_to_app()`, `delay_ms()`, `partition_is_valid()`, PA0 按键检测
- 跳转链路验证通过 ✓（Bootloader→App，SP/PC/VTOR 正确）

### 阶段三：Flash 操作层 ✅

- **`bootloader/flash_control.c`** — 按扇区擦除/写入/字节编程，写入后即时校验

### 阶段四：CRC32 校验 ✅

- **`bootloader/ota_params.c`** — CRC32 查表实现（多项式 0xEDB88320），`crc32_calc()` / `crc32_flash()`

### 阶段五：OTA Parameter 管理 ✅

- **`bootloader/ota_params.c`** — `ota_params_init()`, `ota_params_load()`, `ota_params_save()`
- 加载时防御修正 `max_boot_count=0` 遗留问题

### 阶段六：YMODEM 协议实现 ✅

- **`bootloader/ymodem.c`** (~300 行) — YMODEM-1K 接收实现
  - 持续发 'C' 轮询（100ms 短超时，避免忙等待导致 UART RX 溢出）
  - `ymodem_recv_packet_body()` / `ymodem_recv_packet()` — 包头+序号+数据+CRC16 接收
  - `parse_filename_packet()` — 文件名+大小解析
  - 完整协议：发 'C' → 文件名包 → ACK → 擦 Flash → 发 'C' → 数据包 → EOT×2 → 空文件名包 → ACK
  - **关键设计**：`ymodem_recv_packet_body` 内零 printf，避免 CPU 忙时 USART1 RX 溢出（ORE）

### 阶段七：升级状态机与回滚 ✅

- **`bootloader/boot_main.c`** — `boot_decision()` 三态决策
  - IDLE: 正常启动
  - COMPLETE: boot_count++ → CRC32 校验 → 切换分区 / 回滚
  - FAILED: 自动回滚

### 阶段八：App 侧工程配置 ✅

- **`app/main.c`** — `app_check_ota_params()`, VTOR 自检, 心跳输出

### 阶段九：PC 端发送工具 ✅

- **`tools/ymodem_send.py`** (约 260 行) — Python YMODEM-1K 发送脚本
  - `crc16()`, `send_packet()`, `send_filename_packet()`, `wait_for_byte()`, `wait_for_c()`
  - 命令行：`python tools\ymodem_send.py COM4 mdk\app.bin`

### 阶段十：联调与验证 ✅

- **双串口架构** — USART1(PA9/PA10)=YMODEM 数据传输, USART2(PA2)=Debug printf
  - `driver/usart.c` — 新增 `UART2_Init()`
  - `fputc` 重定向到 USART2，彻底解决 printf 干扰协议的问题
- **端到端传输通过** — 文件名包 + 7 个数据包 + EOT×2 + 空文件名包，全部 CRC 校验通过
- **CRC32 验证** — Flash 内容 CRC32 一致，分区切换正常
- **回滚验证** — max_boot_count 修复后升级确认成功，Boot attempt 1/3 正确
- **App 启动** — 跳转后 App 正常输出 Heartbeat

---

## 双串口架构

```
USART1 (PA9/PA10) ──→ USB 转串口 #1 ──→ PC COM4 ──→ YMODEM 协议传输
USART2 (PA2)       ──→ USB 转串口 #2 ──→ PC COMx ──→ Debug printf 输出
```

所有 `printf` 输出到 USART2，USART1 纯做 YMODEM 协议，互不干扰。

---

## 文件清单

```
app/
└── main.c                ✅

bootloader/
├── boot_config.h         ✅
├── boot_main.c           ✅
├── flash_control.h       ✅
├── flash_control.c       ✅
├── ota_params.h          ✅
├── ota_params.c          ✅（含 max_boot_count 防御修正）
├── ymodem.h              ✅
└── ymodem.c              ✅（clean, ~300 行）

driver/
├── usart.h               ✅（新增 UART2_Init 声明）
└── usart.c               ✅（双串口：USART1=YMODEM, USART2=printf）

mdk/
├── workspace.uvmpw       ✅
├── app.uvprojx           ✅
├── app.sct               ✅
├── bootloader.uvprojx    ✅
└── bootloader.sct        ✅

tools/
└── ymodem_send.py        ✅
```

---

## 问题记录

| 编号 | 问题 | 状态 |
|------|------|------|
| #1 | Workspace 文件格式不兼容 | 已解决 |
| #2 | `fputc` 多重定义 | 已解决 |
| #3 | 旧工程文件残留 | 已解决 |
| #4 | `partition_is_valid()` 未调用 | 已解决 |
| #5 | Bootloader→App 跳转后疑似复位 | 已解决 |
| #6 | `uart_getc_timeout` 未定义 | 已解决 |
| #7 | `delay_loop(80000)` 导致 UART RX 溢出（ORE） | 已解决（改为 100ms 短超时轮询） |
| #8 | 调试 printf 含 'C' 字符触发 Python 误判 | 已解决（改字符串 + 双串口架构） |
| #9 | `max_boot_count` 遗留值为 0 导致立即回滚 | 已解决（`ota_params_load` 防御修正） |
| #10 | 单串口 printf 干扰 YMODEM 协议 | 已解决（双串口架构） |
