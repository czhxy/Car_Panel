# Car_Panel — 汽车双 ECU 仪表盘项目

> STM32F429IGT6（显示域）+ STM32F103C8T6（动力域）+ CAN 总线架构
> LQFP176 板载 SDRAM/NAND，LTDC RGB 屏 + DMA2D + LVGL 全屏双缓冲

## 项目概述

| 角色 | MCU | 职责 |
|------|-----|------|
| **显示域 ECU** | STM32F429IGT6 @ 180 MHz | Bootloader/IAP、LTDC RGB LCD、LVGL 仪表盘、CAN 通信、SPI Flash OTA |
| **动力域 ECU** | STM32F103C8T6 @ 72 MHz | 编码器测速、PWM 电机控制、PID 闭环、CAN 状态上报 |
| **通信总线** | CAN 500 kbps / 29-bit 扩展帧 | 终端电阻 120 Ω × 2（两端） |
| **OTA 升级** | W25Q64 SPI Flash + YMODEM-1K | 双槽位 A/B 分区、CRC32 校验、掉电保护 |

## 仓库结构

```
Car_Panel/
├── README.md                            # 本文件
├── CLAUDE.md                            # AI 上下文指引
├── docs/
│   ├── Car_Panel_Project_Plan.md        # 原 F407 方案文档
│   ├── Car_Panel_F429_Project_Plan.md   # F429 v2.4 完整方案（当前权威）
│   └── schematic/
│       └── stm32f429igt6开发板.pdf       # 开发板原理图
├── project/
│   └── display_ecu_f429/                # 显示域工程（当前开发）
│       ├── app/                         # BSP 驱动层（bsp_key/led/can/log/uart）
│       ├── bootloader/                  # Bootloader（YMODEM + OTA 决策）
│       ├── firmware/                    # CMSIS/Startup/中断向量表
│       ├── task/                        # FreeRTOS 任务（CAN 收发、按键、心跳）
│       ├── third_lib/                   # FreeRTOS v11.3.0
│       ├── mdk/                         # Keil 工程（app + boot）
│       ├── protocol/                    # CAN ID 协议解析（Python 工具）
│       └── tools/                       # Python YMODEM 发送工具
├── power_ecu_f103.todo/                 # 动力域工程（待建设）
├── test_ota_stm32f411/                  # OTA 验证工程（F411, 已通过）
└── tools/                               # 通用工具脚本
```

## 当前进展（显示域 F429）

| 模块 | 状态 |
|------|------|
| Bootloader + 跳转 App | 已完成 |
| FreeRTOS 多任务调度 | 已完成 |
| CAN 收发（500 kbps, 29-bit ext ID） | 已完成 |
| 按键扫描 + 信号量触发 CAN 发送 | 已完成 |
| 心跳 LED + 串口日志 | 已完成 |
| OTA 双分区 A/B（片上 Flash） | 已完成 |
| W25Q64 SPI Flash OTA | Phase 4 |
| LTDC RGB LCD 驱动 | Phase 1 |
| DMA2D + LVGL 仪表盘 | Phase 2–6 |
| 动力域 CAN 闭环 | Phase 5 |

## Flash 分区（F429 片上 1 MB）

```
0x08000000 - 0x0800FFFF   Bootloader       64 KB
0x08010000 - 0x0801FFFF   OTA 参数区       64 KB (逻辑 16 KB)
0x08020000 - 0x0807FFFF   App A           384 KB
0x08080000 - 0x080FFFFF   App B           512 KB
```

## CAN 通信协议摘要

- **帧格式**：29 位扩展帧，500 kbps
- **协议层**：优先级(3 bit) + 源地址(4 bit) + 目标地址(4 bit) + 帧类型(2 bit) + 功能号(10 bit) + 功能字段(6 bit)
- **自节点地址**：`CAN_DEVICE_ID_MAINBOARD = 0x01`
- **滤波器**：当前全通（掩码 0），接收所有报文 → FIFO0

## 开发环境

| 项目 | 配置 |
|------|------|
| IDE | Keil MDK-ARM v5 |
| 调试器 | ST-Link V2 / CMSIS-DAP (SWD) |
| 编译器 | Arm Compiler 6 |
| RTOS | FreeRTOS v11.3.0 (抢占式, heap_4, 64 KB) |
| GUI | LVGL v8（计划） |

## 编译与烧录

### App 工程
1. 打开 `project/display_ecu_f429/mdk/app.uvprojx`
2. 选择 Target: App，编译（F7）
3. SWD 一键烧录（Flash Download 配置：`0x08020000` 起始）

### Bootloader 工程
1. 打开 `project/display_ecu_f429/mdk/boot.uvprojx`
2. 选择 Target: Bootloader，定义 `BOOTLOADER` 宏，链接 `bootloader.sct`
3. 烧录到 `0x08000000`

### 烧录顺序（首次/全擦后）
1. 先烧 Bootloader（`0x08000000`）
2. 再烧 App（`0x08020000`）
3. 首次上电 Bootloader 自动初始化 OTA 参数区

## 编码规范

- BSP 层：`app/bsp_<module>.c/h` — 硬件抽象，纯寄存器操作
- 任务层：`task/mod_<module>.c/h` — 业务逻辑，依赖 FreeRTOS
- 日志：`LOG_E/LOG_W/LOG_I/LOG_D`（`bsp_log.h` 统一宏）
- CAN 协议：ID 位域定义和 API 声明集中在 `task/mod_comm_can.h`
- 注释和文档使用中文

## 参考文档

- 完整方案：[docs/Car_Panel_F429_Project_Plan.md](docs/Car_Panel_F429_Project_Plan.md)（v2.4，当前权威）
- 开发板原理图：[docs/schematic/stm32f429igt6开发板.pdf](docs/schematic/stm32f429igt6开发板.pdf)
- AI 上下文：[CLAUDE.md](CLAUDE.md)
