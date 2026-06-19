# OTA 实现进度

> 最后更新：2026-06-19
> 文档用途：记录当前进度、架构说明、Python 工具使用方法

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

## 架构概览

### Flash 分区布局（STM32F411RET6，512KB）

```
0x08000000 ┌───────────────────────┐
           │     Bootloader         │ 48KB (Sector 0-2，各 16KB)
0x0800C000 ├───────────────────────┤
           │   OTA Parameter        │ 16KB (Sector 3)
0x08010000 ├───────────────────────┤
           │     App A（活跃区）     │ 192KB (Sector 4: 64KB + Sector 5: 128KB)
0x08040000 ├───────────────────────┤
           │     App B（备份区）     │ 256KB (Sector 6: 128KB + Sector 7: 128KB)
0x08080000 └───────────────────────┘
```

### A/B 分区 OTA 升级流程

```
┌─────────────────┐
│ 上电 → Bootloader │
└────────┬────────┘
         ▼
   读取 OTA Parameter
         │
    ┌────┴────┐
    │         │
  IDLE    COMPLETE        FAILED
    │         │              │
    │    boot_count++    自动回滚
    │         │              │
    │    CRC32 校验新固件    │
    │     ┌───┴───┐         │
    │   通过     失败        │
    │     │       │         │
    │  swap A↔B  rollback   │
    │     │       │         │
    └─────┴───────┴─────────┘
         │
    ┌────┴────┐
    │         │
  App 有效   无效
    │         │
  跳转 App  等待 OTA
             (PA0 按键 或 自动)
```

#### 升级完整流程

```
正常运行时:   App A = 活跃，App B = 空闲
                         │
                （PC 端 YMODEM 发送 App B）
                         │
写入完成后:    App A = 活跃，App B = COMPLETE（新固件已写入）
                         │
               （Bootloader 自动复位）
                         │
重启验证:     boot_count=1，CRC32 校验 App B
                         │
    ┌────────────────────┤
    │                    │
 校验通过              校验失败
    │                    │
swap: App B = 活跃    rollback: App A 仍活跃
    │                    │
跳转 App B 运行        跳转 App A 运行（旧固件）
```

---

## Python 发送工具使用方法

### 环境准备

**1. 安装 Python 依赖**

```bash
pip install pyserial
```

**2. 硬件连接**

- USART1（PA9/TX、PA10/RX）通过 USB 转串口模块连接 PC
- PA0 按键（低电平触发），用于进入 OTA 模式
- 波特率 115200，8 数据位，1 停止位，无校验

### 基本用法

```bash
# 语法
python tools\ymodem_send.py <COM端口> <BIN文件路径>

# 示例
python tools\ymodem_send.py COM4 mdk\app.bin
```

### 完整操作步骤

```
步骤 1：编译 App 工程
  → 在 Keil MDK 中打开 mdk\app.uvprojx，编译生成 mdk\app.bin

步骤 2：MCU 上电、进入 Bootloader
  → 方式一：上电后 2 秒内按下 PA0 按键
  → 方式二：如果 App 分区无效，Bootloader 自动进入升级模式
  → 串口助手看到: "[BOOT] Entering upgrade mode, starting YMODEM..."

步骤 3：关闭串口助手（COM 口不能同时被两个程序打开）

步骤 4：在命令行中执行 YMODEM 发送
  python tools\ymodem_send.py COM3 mdk\app.bin

步骤 5：观察输出
  [SEND] File: app.bin, Size: 5500 bytes
  [SEND] Waiting for 'C' from MCU...
  [SEND] Got 'C', starting transmission.
  [SEND] Data transfer starting...
  [SEND] Progress: 1024/5500 (18%)
  ...
  [SEND] Transfer complete!
  SUCCESS: Firmware sent!

步骤 6：MCU 自动复位
  → Bootloader 检测到 OTA_STATE_COMPLETE
  → CRC32 校验通过后切换活跃分区
  → 跳转到新固件运行
```

### 常用命令速查

```bash
# 向 COM3 发送 app.bin（默认波特率 115200）
python tools\ymodem_send.py COM3 mdk\app.bin

# 向 COM4 发送固件
python tools\ymodem_send.py COM4 mdk\app.bin

# 如果固件在别的位置
python tools\ymodem_send.py COM3 ..\other_project\firmware.bin
```

### PC 端传输失败排查

| 现象 | 原因 | 解决 |
|------|------|------|
| `Waiting for 'C'` 一直等待 | MCU 未进入升级模式，或串口号不对 | 检查 COM 口、确认 MCU 打印 `Entering upgrade mode` |
| `ACK timeout` | MCU 在传输中丢失数据（UART RX 溢出） | 检查串口线是否接触良好、确认波特率一致 |
| `Got NAK` | MCU 请求重传 | 可能数据 CRC 不匹配，脚本会自动重试 |
| 串口被占用 | 串口助手未关闭 | 关闭所有使用该 COM 口的程序 |

---

## 已完成详情

### 阶段一：工程配置 ✅

- **`boot_config.h`** — 分区地址宏定义（48KB Bootloader / 16KB Parameter / 192KB App A / 256KB App B）
- **`mdk/bootloader.sct`** — Bootloader 分散加载（0x08000000, 48KB）
- **`mdk/app.sct`** — App 分散加载（0x08010000, 192KB）
- **`mdk/bootloader.uvprojx`** — Bootloader 工程（Define: 含 `BOOTLOADER` 宏）
- **`mdk/app.uvprojx`** — App 工程（Define: `STM32F411xE,...`，无 `BOOTLOADER` 宏）
- **`mdk/workspace.uvmpw`** — 多工程工作区
- **`system_stm32f4xx.c`** — VECT_TAB_OFFSET 按 `BOOTLOADER` 宏区分

### 阶段二：Bootloader 骨架 ✅

- **`bootloader/boot_main.c`** — `jump_to_app()`, `delay_ms()`, `partition_is_valid()`, PA0 按键检测
- 跳转链路验证通过（Bootloader→App，SP/PC/VTOR 正确）

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

- **`tools/ymodem_send.py`** (~260 行) — Python YMODEM-1K 发送脚本
  - `crc16()`, `send_packet()`, `send_filename_packet()`, `wait_for_byte()`, `wait_for_c()`
  - 命令行：`python tools\ymodem_send.py COM3 mdk\app.bin`

### 阶段十：联调与验证 ✅

- **端到端传输通过** — 文件名包 + 数据包 + EOT×2 + 空文件名包，全部 CRC 校验通过
- **CRC32 验证** — Flash 内容 CRC32 一致，分区切换正常
- **回滚验证** — max_boot_count 修复后升级确认成功，Boot attempt 正确
- **App 启动** — 跳转后 App 正常输出 Heartbeat

---

## 实际工程文件清单

```
test_ota_stm32f411/
├── app/
│   └── main.c                    ✅ App 入口（OTA 参数检查 + 心跳）
│
├── bootloader/
│   ├── boot_config.h             ✅ Flash 分区/状态常量
│   ├── boot_main.c               ✅ Bootloader 主逻辑（状态机 + 跳转 + YMODEM）
│   ├── flash_control.h           ✅ Flash 操作接口
│   ├── flash_control.c           ✅ Flash 擦写实现
│   ├── ota_params.h              ✅ OTA 参数结构体 + CRC32 声明
│   ├── ota_params.c              ✅ CRC32 + 参数管理（含 max_boot_count 防御）
│   ├── ymodem.h                  ✅ YMODEM 协议接口
│   └── ymodem.c                  ✅ YMODEM-1K 接收实现（~300 行）
│
├── driver/
│   ├── usart.h                   ✅ UART 驱动声明
│   └── usart.c                   ✅ USART1 初始化 + fputc 重定向
│
├── firmware/
│   ├── cmsis/
│   │   ├── core/                 ✅ CMSIS-CORE（core_cm4.h 等）
│   │   └── device/               ✅ 启动文件 + system_stm32f4xx + 中断处理
│   └── driver/
│       ├── inc/                  ✅ STM32F4 标准外设库头文件（40 个）
│       └── src/                  ✅ STM32F4 标准外设库源文件（40 个）
│
├── mdk/
│   ├── workspace.uvmpw           ✅ 多工程工作区
│   ├── app.uvprojx               ✅ App Keil 工程（Target: 0x08010000）
│   ├── app.sct                   ✅ App 分散加载文件
│   ├── bootloader.uvprojx        ✅ Bootloader Keil 工程（Target: 0x08000000）
│   ├── bootloader.sct            ✅ Bootloader 分散加载文件
│   ├── app.bin                   ✅ App 编译产物（用于 YMODEM 发送）
│   └── Objects/                  ✅ 编译中间产物（.o/.axf/.hex）
│
├── tools/
│   └── ymodem_send.py            ✅ Python YMODEM-1K 发送工具
│
├── third_lib/                    （预留，当前为空）
├── ota分区规划.md                 ✅ OTA 分区设计文档（详细实现指南）
└── ota进度.md                     ✅ 本文件
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
| #8 | 调试 printf 含 'C' 字符触发 Python 误判 | 已解决（改字符串 + 双串口调试架构） |
| #9 | `max_boot_count` 遗留值为 0 导致立即回滚 | 已解决（`ota_params_load` 防御修正） |
| #10 | 单串口 printf 干扰 YMODEM 协议 | 已解决（双串口调试架构，生产环境仅用单串口） |

---

## 关键设计规则

1. **`ymodem_recv_packet_body` 内禁止 printf**：STM32F4 USART 仅单字节 RX buffer，printf 耗时（~2ms）会导致 ORE（过载错误）
2. **Bootloader 不中断运行**：SysTick 轮询延时、无中断，避免 Flash 擦写期间访问冲突
3. **Flash 先擦后写**：STM32F4 Flash 写只能 1→0，擦除是 0→1；擦除最小单位是 Sector（16KB/64KB/128KB）
4. **VTOR 由 Bootloader 设置**：App 的 `SystemInit()` 不覆盖 VTOR，向量表地址由 Bootloader 跳转前设置
5. **OTA 参数保存后立即复位**：升级完成后调用 `NVIC_SystemReset()`，让新固件在干净环境中启动
