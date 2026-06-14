# Car_Panel 汽车双 ECU 仪表盘项目方案（优化版 v2.0）

> 项目代号：**Car_Panel**
> 文档版本：v2.1（基于 v2.0 方案 + 完善修订）
> 目标：在双 MCU（STM32F407 + STM32F103）+ CAN 总线 架构上，模拟汽车"动力域 ECU + 显示域 ECU"，并实现 OTA/IAP、LVGL 仪表盘 UI、闭环电机控制。
> 本方案独立成文，不在 v1.0 文档上覆盖；v1.0 仅作历史参考保留。

---

## 目录

1. [项目目标与变更说明](#1-项目目标与变更说明)
2. [硬件资源清单与角色分配](#2-硬件资源清单与角色分配)
3. [F407 引脚规划（显示域）](#3-f407-引脚规划显示域)
4. [F103C8T6 引脚规划（动力域）](#4-f103c8t6-引脚规划动力域)
5. [电源树设计](#5-电源树设计)
6. [Flash 与外部存储分区](#6-flash-与外部存储分区)
7. [主方案系统架构](#7-主方案系统架构)
8. [CAN 通信协议草案](#8-can-通信协议草案)
9. [软件模块拆分](#9-软件模块拆分)
10. [OTA/IAP 实现路线](#10-otaiap-实现路线)
11. [LVGL 仪表盘页面规划](#11-lvgl-仪表盘页面规划)
12. [安全策略与故障处理](#12-安全策略与故障处理)
13. [采购清单（修正版）](#13-采购清单修正版)
14. [开发阶段计划](#14-开发阶段计划)
15. [下一步行动清单](#15-下一步行动清单)
16. [调试策略](#16-调试策略)
17. [测试计划](#17-测试计划)
18. [版本控制与仓库管理](#18-版本控制与仓库管理)

---

## 1. 项目目标与变更说明

### 1.1 项目目标

构建"双 ECU + CAN + LVGL + OTA"的桌面级汽车域控演示系统：

- **动力域 ECU**：STM32F103C8T6 采集编码器测速、PWM 控制电机、故障检测，周期性通过 CAN 发送状态；接收显示域命令控制启停/目标转速。
- **显示域 ECU**：STM32F407ZGT6 运行 Bootloader/IAP + LVGL 仪表盘，通过 CAN 接收动力域数据并实时显示；支持 W25Q64 外置 Flash 暂存固件 + AT24C02 存放升级标志。
- **通信总线**：500 kbps CAN（终端电阻 120 Ω 两端匹配），贴近真实车载 ECU。
- **OTA 升级**：第一版 `串口/YMODEM → W25Q64 → Bootloader 搬运`；第二版 App 接收 + Bootloader 搬运；第三版 ESP32-DOWN-V3 作 WiFi 网关。

### 1.2 相对 v1.0 的主要变更

| # | 变更项 | v1.0 | v2.0（本方案） | 变更原因 |
|---|--------|------|----------------|----------|
| 1 | F407 Flash 分区 | 0x080F8000 起 32 KB 参数区（不在扇区边界） | 全部对齐 STM32F4 扇区边界 | OTA 擦写会破坏相邻区域，必须修正 |
| 2 | F103 型号 | 笼统"F103 核心板" | 明确 STM32F103C8T6（64 KB Flash / 20 KB RAM） | 资源评估和引脚规划需要明确型号 |
| 3 | 板载 Flash | 写 W25Q64（8 MB） | 实际为 **W25Q16（2 MB）**，外置 W25Q64（8 MB） | 原理图确认板载是 W25Q16 |
| 4 | SDRAM | 隐含假设可用 | **板载 SDRAM 预留未焊接**，实际无 SDRAM | 原理图标注 "预留未焊接" |
| 5 | CAN1 引脚 | 未明确 | 明确使用 PB8/PB9 复用（避开 USB 和 FSMC） | F407 板上 PA11/12 已被 USB 占用，PD0/PD1 已被 FSMC 占用 |
| 6 | 外部 W25Q64 接入 | 未明确 | SPI3 (PC10/11/12) + 独立 CS | SPI1 已被 W25Q16+触摸屏占用；SPI2 引脚冲突；SD 卡槽不焊 |
| 7 | I2C 接入 | 未明确 | I2C3 (PA8/PC9) 接 AT24C02 | 避开 CAN1 复用脚和 FSMC 地址线 |
| 8 | LVGL 资源 | 未明确存储位置 | 中文字体 / 大图片放 W25Q64，ASCII 点阵可放片上 | 片上 Flash 紧张（480 KB App 区） |
| 9 | RTOS | 未提 | F407 上 FreeRTOS，F103 上裸机 | 多任务并存（CAN/LVGL/Flash/按键/日志）裸机难以调度 |
| 10 | 电源设计 | 仅提"12V 2A 适配器" | 给出完整电源树：12V → DCDC(5V) → LDO(3.3V)，电机与 MCU 分通道 | 电机噪声会通过地耦合干扰 MCU |
| 11 | 引脚规划 | 无 | 完整引脚分配表 + 冲突检查 | 原理图已读，按实际板载资源规划 |
| 12 | 电源/地隔离 | 未提 | 电机入口加大电容、编码器信号双绞线 + 屏蔽、12V 入口 π 型滤波 | 防止电机换向/堵转的尖峰耦合到 MCU |
| 13 | 缺省安全状态 | 未提 | 上电电机停止；通信超时自动停机；IWDG 必开；软启动 ramp | 桌面/调试阶段误操作可能损坏电机/电源 |
| 14 | CAN 超时阈值 | 未给 | 显示域 100 ms 未收到 0x180 → 离线；动力域 200 ms 未收到 0x200 → 停机 | 给出可实现的具体值 |
| 15 | W25Q16 用途 | 笼统"固件暂存" | 板载 W25Q16（2 MB）→ 仪表盘字库 + 关键图标；外置 W25Q64（8 MB）→ OTA 暂存 + 资源 | 充分利用板载 Flash 节省外置芯片占用 |
| 16 | OTA 顺序 | Phase 4 | 提前到 Phase 2 | 反复烧录 App 时没有 IAP 效率太低 |
| 17 | Git 仓库 | 未提 | 建议建仓 + 分支策略 | 双工程多分支，强烈建议版本管理 |
| 18 | 串口日志规范 | 未提 | 统一 `bsp_log.h`，带模块/等级/时间戳 | 多模块联调时日志会非常多 |

---

## 2. 硬件资源清单与角色分配

### 2.1 STM32F407ZGT6 显示域（已购开发板）

> 资源清单来自随板原理图 `STM32F407Z开发板--原理图-2019M(新液晶接口).PDF`。

| 资源 | 规格 | 备注 |
|------|------|------|
| 主频 | 168 MHz | 跑 LVGL 足够 |
| Flash | 1 MB（片上） | 0x08000000–0x080FFFFF |
| RAM | 192 KB（片上） | 不够做全屏双缓冲，必须局部刷新 |
| FSMC | 有 | 驱动 8080 并口屏 |
| bxCAN | CAN1 + CAN2 | 接 CAN 收发器 |
| SPI | SPI1/2/3 | 注意引脚分配 |
| I2C | I2C1/2/3 | 注意引脚冲突 |
| 板载 Flash | **W25Q16（2 MB / 16 Mbit）** | 挂 SPI1，CS=PB14 |
| 板载 SDRAM | **IS62WV51216（1 MB）— 预留未焊接** | 实际无 SDRAM，不能依赖 |
| SD 卡槽 | 有（U5，SDIO） | 用户没卡，可借用其引脚 |
| NRF24L01 接口 | J8（SPI1） | 本项目不用 |
| 板载 LED | PF9 / PF10 | LED0 / LED1 |
| 板载按键 | PE3 / PE4 / PA0 | KEY0 / KEY1 / WK_UP |
| USB | PA11/PA12（USB FS Slave） | 与 CAN1 默认脚冲突，CAN1 走 PB8/9 |
| USART1 | PA9/PA10（ISP 口） | 本方案用作 Debug 串口 |
| 供电 | 5V 入口 → AMS1117-3.3V LDO（板上） | 5V 可从外置 5V DCDC 注入 |

**角色：显示域 ECU**。运行 Bootloader/IAP + LVGL 仪表盘；外置 W25Q64 用于 OTA 暂存 + 资源；AT24C02 用于升级标志。

### 2.2 STM32F103C8T6 动力域核心板

| 资源 | 规格 | 备注 |
|------|------|------|
| 主频 | 72 MHz | 跑电机控制足够 |
| Flash | 64 KB | 单 App，无 Bootloader |
| RAM | 20 KB | 裸机 + 几任务足够 |
| TIM | TIM1/2/3/4 | 1 路 PWM + 1 路编码器 |
| bxCAN | CAN1（PA11/12 默认） | 无冲突 |
| USART | USART1/2/3 | 调试 USART1 足够 |
| 封装 | LQFP48 | 引脚有限 |
| 板载晶振 | 8 MHz HSE + 32.768 kHz LSE | 标准配置 |

**角色：动力域 ECU**。闭环控制电机；CAN 与 F407 通信；故障检测与降级。

### 2.3 外置模块

- **W25Q64（8 MB / 64 Mbit）** SPI Flash 模块 — 外置，接 SPI3
- **AT24C02（256 Byte）** I2C EEPROM — 外置，接 I2C3
- **TJA1050 / SN65HVD230 CAN 收发器** ×2（已有一块，再买一块）
- **BTS7960 / IBT-2 H 桥** — 电机驱动
- **JGA25-370 带 AB 相霍尔编码器电机**（12 V）— 第一版推荐
- **3.5 寸 480×320 8080 并口 TFT，ILI9488/ILI9486**（带或不带触摸）

### 2.4 2208 无刷电机套件

**第一版不使用**。F103C8T6 资源不够做 FOC/六步换相，且本项目重点在双 ECU 架构/CAN/LVGL/OTA。保留为后续"进阶动力域"扩展。

---

## 3. F407 引脚规划（显示域）

### 3.1 引脚分配总表

| 模块 / 信号 | MCU 引脚 | 备注 |
|-------------|---------|------|
| **电源 / 系统** | | |
| 5V 入口 | 5V 引脚 | 来自外置 5V DCDC |
| 3V3 | AMS1117 输出 | 板上 LDO 已就绪 |
| Vbat | VBAT | 板上 CR1220 电池 |
| NRST | NRST | 板上复位 |
| BOOT0 | BOOT0 | 跳线帽默认 0 |
| **SWD 调试** | | |
| SWDIO | PA13 | |
| SWCLK | PA14 | |
| **USART1 调试日志** | | |
| USART1_TX | PA9 | |
| USART1_RX | PA10 | |
| **CAN1 总线** | | |
| CAN1_RX | PB8 | remap 1 |
| CAN1_TX | PB9 | remap 1 |
| **FSMC 8080 LCD（PD/PE/PF/PG）** | | |
| FSMC_D0 | PD14 | |
| FSMC_D1 | PD15 | |
| FSMC_D2 | PD0 | |
| FSMC_D3 | PD1 | |
| FSMC_D4 | PE7 | |
| FSMC_D5 | PE8 | |
| FSMC_D6 | PE9 | |
| FSMC_D7 | PE10 | |
| FSMC_D8 | PE11 | |
| FSMC_D9 | PE12 | |
| FSMC_D10 | PE13 | |
| FSMC_D11 | PE14 | |
| FSMC_D12 | PE15 | |
| FSMC_D13 | PD8 | |
| FSMC_D14 | PD9 | |
| FSMC_D15 | PD10 | |
| FSMC_NOE (RD) | PD4 | |
| FSMC_NWE (WR) | PD5 | |
| FSMC_NE4 (CS) | PG12 | |
| FSMC_A6 (RS/DC) | PF12 | 数据/命令选择（原理图液晶接口用 A6 非 A0） |
| LCD_RESET | PF11 | 单独 GPIO，不用 NRST |
| LCD_BL | PB15 | 板载 Q2 PNP，默认高电平亮 |
| **XPT2046 触摸（SPI1，与 W25Q16 共享总线）** | | |
| SPI1_SCK (T_SCK) | PB3 | 与 W25Q16 共享 |
| SPI1_MISO (T_MISO) | PB4 | 与 W25Q16 共享 |
| SPI1_MOSI (T_MOSI) | PB5 | 与 W25Q16 共享 |
| T_CS | PA4 | 触摸 CS |
| T_PEN | PA5（备用） | 触摸中断，可选 |
| **板载 W25Q16（2 MB，SPI1 共享）** | | |
| W25Q16_CS (F_CS) | PB14 | 板上固定 |
| **外置 W25Q64（8 MB，SPI3 独立）** | | |
| SPI3_SCK | PC10 | SD 卡脚复用（不焊卡） |
| SPI3_MISO | PC11 | |
| SPI3_MOSI | PC12 | |
| W25Q64_CS | PC4 | 任意空闲 GPIO |
| **AT24C02（I2C3）** | | |
| I2C3_SCL | PA8 | |
| I2C3_SDA | PC9 | SD 卡 DAT1 脚（不焊卡） |
| **板载 LED** | | |
| LED0（系统心跳） | PF9 | **低电平点亮**（板上 LED 阳极接 VCC，阴极经电阻到 GPIO） |
| LED1（CAN 状态） | PF10 | **低电平点亮**（同 LED0） |
| **板载按键** | | |
| KEY0（页面切换） | PE3 | 低电平有效 |
| KEY1（启停/OTA） | PE4 | 低电平有效 |
| WK_UP（强制 OTA） | PA0 | 高电平有效 |
| **未使用 / 备用** | | |
| PA1, PA2, PA3, PA6, PA7, PA15, PB0, PB1, PB2, PB10, PB11, PB12, PB13, PC0, PC1, PC2, PC3, PC5, PC6, PC7, PC13, PE0, PE1, PE2, PE5, PE6, PF6, PF7, PF8, PA11, PA12 | 留作扩展 | USB 引脚不引出，可作 GPIO |

### 3.2 引脚冲突检查（关键）

| 冲突点 | 处理 |
|--------|------|
| PA11/PA12 同时是 USB 和 CAN1 默认脚 | CAN1 走 PB8/PB9 remap，USB 不用 |
| PD0/PD1 同时是 FSMC_D2/D3 和 CAN1 remap2 脚 | CAN1 已用 remap1，PD0/1 给 LCD |
| PB8/PB9 同时是 CAN1 remap 和 I2C1 | CAN1 优先级，I2C 用 I2C3 |
| PB3/PB4/PB5 同时是 SPI1 和 JTAG 调试脚 | SPI1 在不用 SWD 调试时是 OK 的；调试时 SWD 走 PA13/PA14 即可 |
| PB14 同时是 W25Q16 CS 和"板上 F_CS"标注 | 给板载 W25Q16，触摸 CS 用 PA4 |
| PB15 同时是 LCD_BL 和 SPI2_MOSI | LCD_BL 优先，SPI2 不用 |
| PC8–PC12 同时是 SDIO 和 SPI3 | 第一版不焊 SD 卡，PC10/11/12 给 SPI3 |
| PF0/PF1 同时是 FSMC_A0/A1 和 I2C2_SDA/SCL | LCD RS 已用 A6(PF12)，A0/A1 不占用；I2C 仍用 I2C3，无冲突 |
| PC9 同时是 SDIO_D1 和 I2C3_SDA（默认） | 不焊 SD 卡，PC9 给 I2C3_SDA |

**结论：无未解决的冲突。**

### 3.3 外接 8080 LCD 模块推荐接法（J6 排线顺序参考）

| LCD 引脚 | 接到 |
|----------|------|
| D0–D15 | F407 PD0/1/8/9/10/14/15 + PE7–15（FSMC 自动映射） |
| /CS | PG12（FSMC_NE4） |
| /WR | PD5（FSMC_NWE） |
| /RD | PD4（FSMC_NOE） |
| RS (DC) | PF12（FSMC_A6，地址线兼任数据/命令选择） |
| /RST | PF11 |
| BL | PB15（Q2 控制） |
| T_SCK/MISO/MOSI | PB3/PB4/PB5（SPI1 共享） |
| T_CS | PA4 |
| T_PEN | PA5（可选） |

> 关键：FSMC 配 8080 模式，地址线 A6 作 RS，数据宽度 16 bit。A6=HADDR[7]，所以命令地址偏移 0x00，数据地址偏移 0x80。

### 3.4 FSMC 时序参数参考（8080 LCD）

> F407 主频 168 MHz → HCLK = 168 MHz → 1 个 HCLK ≈ 5.95 ns。FSMC 时序以 HCLK 为单位。

```c
// FSMC_NORSRAM_InitTypeDef / FSMC_NORSRAM_TimingInitTypeDef
// Bank 1, Subbank 4 (NE4 → PG12)
// 数据宽度 16 bit，地址线 A6 (PF12) 作 RS

// 读时序（LCD -> MCU，一般较慢）
read_timing.AddressSetupTime    = 0;   // ADDSET：0 HCLK ≈ 0 ns
read_timing.AddressHoldTime     = 0;
read_timing.DataSetupTime       = 15;  // DATAST：15 HCLK ≈ 89 ns（ILI9488 最小 80 ns）
read_timing.BusTurnAroundDuration = 0;
read_timing.CLKDivision         = 2;
read_timing.DataLatency         = 2;
read_timing.AccessMode          = FSMC_ACCESS_MODE_A;  // Mode A 适合 SRAM/LCD

// 写时序（MCU -> LCD，通常更快）
write_timing.AddressSetupTime   = 0;
write_timing.AddressHoldTime    = 0;
write_timing.DataSetupTime      = 10;  // 10 HCLK ≈ 60 ns（ILI9488 最小 15 ns）
write_timing.BusTurnAroundDuration = 0;
write_timing.CLKDivision        = 2;
write_timing.DataLatency        = 2;
write_timing.AccessMode         = FSMC_ACCESS_MODE_A;

// 写入数据时：
// 命令地址 = 0x6C000000（FSMC Bank1 Subbank4，A6=0）
// 数据地址 = 0x6C000080（FSMC Bank1 Subbank4，A6=1；16bit 模式下 A6=HADDR[7]，偏移 0x80）
#define LCD_CMD_ADDR  (*((__IO uint16_t*)0x6C000000))
#define LCD_DATA_ADDR (*((__IO uint16_t*)0x6C000080))  // A6=1 → HADDR[7]=1 → offset=0x80
```

> **调试提示**：如果 LCD 白屏无显示，先检查 FSMC 地址映射（Bank1 NE4 = 0x6C000000）；如果花屏，检查 DataSetupTime 是否太小。

---

## 4. F103C8T6 引脚规划（动力域）

### 4.1 引脚分配总表

| 模块 / 信号 | MCU 引脚 | 备注 |
|-------------|---------|------|
| **电源 / 系统** | | |
| 5V 入口 | 5V 引脚 | 来自外置 5V DCDC |
| 3V3 | AMS1117 输出（板上） | |
| NRST | NRST | |
| BOOT0 | BOOT0 | 跳线帽默认 0 |
| **SWD 调试** | | |
| SWDIO | PA13 | |
| SWCLK | PA14 | |
| **USART1 调试日志** | | |
| USART1_TX | PA9 | |
| USART1_RX | PA10 | |
| **CAN1 总线** | | |
| CAN1_RX | PA11 | 默认脚 |
| CAN1_TX | PA12 | 默认脚 |
| **电机控制** | | |
| TIM1_CH1 (PWM) | **PA8** | 主 PWM 输出，驱动 BTS7960 L_PWM |
| TIM1_CH1N | PB13 | 互补输出（可选），本方案用单路 |
| DIR 方向控制 | **PA4** | 普通 GPIO，推挽输出，驱动 BTS7960 R_EN |
| EN 使能 | **PB0** | 普通 GPIO，推挽输出，驱动 BTS7960 L_EN |
| **编码器** | | |
| TIM2_CH1 (Encoder A) | **PA0** | TIM2 编码器模式 CH1 |
| TIM2_CH2 (Encoder B) | **PA1** | TIM2 编码器模式 CH2 |
| **状态指示** | | |
| LED_RUN | **PC13** | 低电平点亮（板上 LED，反接也行） |
| LED_FAULT | **PB1** | 高电平点亮 |
| **按键** | | |
| KEY_LOCAL | **PB2** | 启动/停止（低电平有效）；注意 PB2 是 BOOT1，但 BOOT1 仅在上电复位时采样，运行时可作普通 IO |
| **可选：模拟量采集** | | |
| ADC1_IN0 (电压) | PA0 | ⚠ 与 Encoder A 冲突；可改 PB0（但与 EN 冲突） |
|  |  | 第一版可不接，电压/电流从 CAN 模拟 |
| **未使用 / 备用** | | |
| PA2, PA3, PA5, PA6, PA7, PA15, PB3, PB4, PB5, PB6, PB7, PB8, PB9, PB10, PB11, PB12, PB14, PB15 | 留作扩展 | |

### 4.2 编码器测速配置

- TIM2 配置为 Encoder mode 3（CH1、CH2 都计数）
- ARR = 65535（16 位满量程），PSC = 0
- 每 10 ms 读取一次 CNT 增量，转换为 RPM：

```
RPM = (Δcnt / PPR_motor) × (60 000 / 间隔ms)
```

> JGA25-370 典型参数：电机轴 11 PPR × 减速比 30 = 330 PPR 输出轴
> 即输出轴每转 330 个脉冲，对应电机轴 11 个脉冲（编码器在电机侧）

### 4.3 PWM 配置

- TIM1_CH1 (PA8) — PWM mode 1，频率 20 kHz（超出电机可听频率）
- ARR = 3600 - 1（72 MHz / 20 kHz = 3600），CCR = duty × (ARR+1)
- 占空比 0–100% 对应 0–100% 转速命令

### 4.4 关键提醒

- **F103 上没有 SDRAM、没有大 Flash**，App 单工程就行，**不需要 Bootloader**。
- CAN1 默认脚 PA11/12 在 F103 上无冲突，**不要 remap**。
- TIM2 编码器模式只能用 PA0/PA1，**不要用 PA6/PA7**（那是 TIM3）。
- BOOT1 (PB2) 启动后作普通 GPIO 用，**没有副作用**。

---

## 5. 电源树设计

### 5.1 电源架构总图

```
                        12V / 5A DC 适配器
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
  电机电源 (12V)      BTS7960 VCC (5V)        F407/F103 调试口 (5V)
   ┊                     │                     (USB-TTL 模块, 可选)
   ┊                     │
   ┊               12V→5V 隔离/非隔离 DCDC
   ┊                  (LM2596 或 MP1584, ≥3A)
   ┊                     │
   ┊                     ▼
   ┊                  5V 电源轨
   ┊          ┌──────────┼──────────┐
   ┊          │          │          │
   ┊          ▼          ▼          ▼
   ┊     BTS7960_VCC  TJA1050_VCC  屏幕背光/VCC
   ┊          │
   ┊          ▼
   ┊   F407 板上 AMS1117-3.3 LDO  (5V→3.3V, 800mA)
   ┊          │
   ┊          ▼
   ┊       3.3V 电源轨
   ┊   ┌─────┼──────────┬──────────┬────────────┐
   ┊   │     │          │          │            │
   ┊   ▼     ▼          ▼          ▼            ▼
   ┊  F407  F103    W25Q16/64   AT24C02    LCD 逻辑
   ┊
   ▼
电机 (12V) ←  BTS7960 H 桥 ←  PWM+DIR (来自 F103)
```

### 5.2 关键节点参数

| 节点 | 电压 | 电流估算 | 来源 | 备注 |
|------|------|---------|------|------|
| DC 适配器入口 | 12V | 峰值 5A | 外部 12V/5A 开关电源 | 留 2–3 倍电机堵转电流余量 |
| BTS7960 VM（电机电源） | 12V | 持续 2A，峰值 5A | DC 适配器直通 | **必须加 1000 μF/25V + 0.1 μF 解耦** |
| BTS7960 VCC（逻辑） | 5V | < 100 mA | DCDC 5V | |
| DCDC 5V 输出 | 5V | < 1A（系统总） | LM2596 / MP1584 模块 | 输入 12V，输出 5V/3A |
| F407 板上 3.3V | 3.3V | < 500 mA | AMS1117-3.3（板上） | 给所有 3.3V 逻辑供电 |
| TJA1050 VCC | 5V | < 50 mA | DCDC 5V | F103 端同理 |
| LCD 背光 | 5V 或 3.3V | < 200 mA | 视屏而定；多数 3.5 寸屏 3.3V 即可 | PB15 控制 Q2 通断 |

### 5.3 推荐采购电源器件

| 器件 | 推荐型号 | 数量 | 备注 |
|------|---------|------|------|
| 12V 适配器 | 12V/5A 开关电源 | 1 | 5.5×2.1 DC 头 |
| 12V → 5V DCDC 模块 | LM2596S 模块（带电感，可调） | 1 | 输入 12V，输出调到 5V，输出电流 3A |
| 备选 DCDC | MP1584EN 模块 | 1 | 体积小、效率高 |
| 5V → 3.3V LDO | AMS1117-3.3 | — | F407 板载已有，可不外加 |
| 电机电源解耦电容 | 1000 μF/25V 电解 + 0.1 μF 陶瓷 | 1 组 | BTS7960 VM 入口 |
| 5V 电源解耦 | 100 μF/10V + 0.1 μF | 1 组 | DCDC 输出端 |
| 3.3V 电源解耦 | 0.1 μF（每个 IC 旁） | 若干 | 标准做法 |
| 编码器信号滤波 | 100 Ω 串联 + 0.1 μF 到地 | 2 路 | 抑制电机噪声 |

### 5.4 隔离与抗干扰要点

1. **电机电源入口**必须有 1000 μF 大电容 + 0.1 μF 瓷片，否则堵转瞬间会让 12V 跌落导致 MCU 复位。
2. **DCDC 5V 选型**：
   - 非隔离（LM2596/MP1584）：便宜，但高频开关噪声会传导到 5V 轨，需要 π 型滤波（LC）抑制。
   - 隔离（B0505S-1W / B1212S-1W）：贵（且 1W 功率较小），但能彻底隔断电机侧噪声。
   - **本方案推荐非隔离 + LC 滤波**，性价比最佳。
3. **编码器信号**走双绞线或屏蔽线，长度 < 30 cm；信号上串 100 Ω 电阻 + 0.1 μF 到地。
4. **共地问题**：12V、5V、3.3V、BTS7960、MCU 必须共地（接在电源入口的同一接地点），避免浮地。
5. **CAN 总线**两端各加 120 Ω 终端电阻，CANH/CANL 走双绞线，远离电机电源线。

### 5.5 上电时序与保护

- 适配器上电 → DCDC 5V 稳定 → AMS1117 3.3V 稳定 → MCU 复位 → 读取 AT24C02 升级标志 → 进入 Bootloader 或 App
- **电机不会自启动**：默认 `Motor_Command.enable=0`，只有显示域发送 `enable=1` 才转动
- 急停：显示域发 `enable=0` 或按住 F103 的 KEY_LOCAL (PB2) 3 秒 → 停机

---

## 6. Flash 与外部存储分区

### 6.1 STM32F407ZGT6 片上 Flash 分区（修正 v1.0 错误）

> F407ZGT6 扇区结构：4 × 16 KB + 1 × 64 KB + 7 × 128 KB = 1024 KB

**第一版（单 App，无 A/B）：**

```
0x08000000 - 0x0801FFFF   Bootloader    128 KB   (扇区 0–4)
0x08020000 - 0x080DFFFF   App            768 KB  (扇区 5–10)
0x080E0000 - 0x080FFFFF   参数/标志区    128 KB  (扇区 11)
```

**第二版（加 A/B 回滚）：**

```
0x08000000 - 0x0801FFFF   Bootloader    128 KB   (扇区 0–4)
0x08020000 - 0x0807FFFF   App A         384 KB  (扇区 5–7)
0x08080000 - 0x080DFFFF   App B         384 KB  (扇区 8–10)
0x080E0000 - 0x080FFFFF   参数/标志区    128 KB  (扇区 11)
```

**Keil 分散加载示例（第一版，Bootloader 与 App 各自独立工程）：**

```c
// ===== bootloader.sct（Bootloader 工程） =====
LR_IROM1 0x08000000 0x00020000  {    ; Bootloader 128 KB
  ER_IROM1 0x08000000 0x00020000  {
    *.o (RESET, +First)
    startup_*.o (+RO)
    .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00030000  {  ; 192 KB RAM（Bootloader 全部可用）
    .ANY (+RW +ZI)
  }
}

// ===== app.sct（App 工程） =====
LR_IROM1 0x08020000 0x000C0000  {    ; App 768 KB（从 0x08020000 开始）
  ER_IROM1 0x08020000 0x000C0000  {
    *.o (RESET, +First)
    startup_*.o (+RO)
    .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00030000  {  ; 192 KB RAM（App 全部可用）
    .ANY (+RW +ZI)
  }
}

// 注意：参数区 0x080E0000 由 Bootloader/App 通过 IAP 函数按扇区擦写，
//       不参与任何工程的链接过程。
```

### 6.2 STM32F103C8T6 片上 Flash 分区

F103C8T6 总共 64 KB，无 Bootloader 需求（开发阶段 ST-Link 直接烧）：

```
0x08000000 - 0x0800FFFF   App            64 KB  (整片)
```

> 若以后加 CAN Bootloader 再划 0x08000000–0x08007FFF（32 KB）做 Bootloader，App 占 0x08008000–0x0800FFFF（32 KB）。

### 6.3 外置 W25Q64（8 MB）分区

```
0x000000 - 0x0FFFFF   固件暂存 A    1 MB   (F407 App 完整镜像)
0x100000 - 0x1FFFFF   固件暂存 B    1 MB   (第二阶段 OTA 备份)
0x200000 - 0x4FFFFF   LVGL 资源区   3 MB   (图片、字库、图标)
0x500000 - 0x7BFFFF   日志/诊断区   ~2.75 MB
0x7C0000 - 0x7DFFFF   资源元数据     128 KB  (版本号、CRC、长度表)
0x7E0000 - 0x7FFFFF   元数据/参数备份 128 KB
```

### 6.4 板载 W25Q16（2 MB）分区

```
0x000000 - 0x07FFFF   字库 (小字库、ASCII、UI 标签)  512 KB
0x080000 - 0x0FFFFF   图标/启动画面                   512 KB
0x100000 - 0x17FFFF   升级日志 / 关键参数快照          512 KB
0x180000 - 0x1FFFFF   预留 / OTA 暂存 (与外置 W25Q64 互备)
```

> 板载 W25Q16 走 SPI1，调试时直接读/写很方便；外置 W25Q64 走 SPI3，速率可拉满。

### 6.5 AT24C02 数据规划

```
0x00 - 0x03   magic                       "ECUP" = 0x45435550
0x04 - 0x07   boot_count
0x08 - 0x0B   current_version             (例如 0x00010000 = v1.0.0)
0x0C - 0x0F   pending_version
0x10 - 0x13   pending_crc32
0x14 - 0x17   pending_size
0x18          upgrade_request              0=无 1=App A 2=App B
0x19          last_upgrade_result          0=未升级 1=成功 2=失败 3=回滚
0x1A          rollback_request
0x1B - 0x1F   reserved
0x20 - 0xFF   业务参数 (屏幕亮度、CAN 节点 ID、电机标定等)
```

> AT24C02 每页 8 字节，写入 5 ms；建议使用页写 + 读-改-写策略。
> magic 必须写入并校验，避免冷启动读到全 0xFF 误判为首次升级。

---

## 7. 主方案系统架构

```text
                CAN 总线 (500 kbps)
       CANH/CANL + 两端 120 Ω 终端
                    │
    ┌───────────────┴───────────────┐
    │                               │
    ▼                               ▼
+-----------+                 +-----------+
| 显示域 ECU|                 | 动力域 ECU|
| STM32F407 |                 | STM32F103 |
|           |                 |           |
| Boot      |                 | 电机控制  |
| LVGL      | <-------------> | PWM 驱动  |
| CAN 接收  |                 | 编码器测速|
| 触摸/按键 |                 | PID 闭环  |
| W25Q16/64 |                 | 故障检测  |
| AT24C02   |                 |           |
+-----+-----+                 +-----+-----+
      │                             │
      │ 8080 并口                   │ PWM/DIR/Encoder
      ▼                             ▼
  3.5寸 8080 屏              DC 减速电机 + H 桥
  (ILI9488)                  (BTS7960 + JGA25-370)
```

**关键流程**：

1. 上电 → F407 Bootloader 检查 AT24C02 → 有升级请求则从 W25Q64 搬运新固件 → 跳转 App
2. App 初始化 LVGL、CAN、显示主仪表页
3. F103 上电 → 默认停机 → 等 CAN 命令
4. F407 周期发 `Motor_Command` (启停/目标转速) → F103 收到后控制电机
5. F103 周期发 `Motor_Status` (实际转速/状态/故障) → F407 显示
6. 用户在 LVGL 上按 KEY1 长按 → 进入 OTA 页面 → 串口 YMODEM 接收固件到 W25Q64 → 复位 → Bootloader 升级

---

## 8. CAN 通信协议草案

### 8.1 波特率与时序

- 波特率：**500 kbps**
- CAN 节点：F407 显示域为 **0x0A**（发送 0x2xx 帧，接收 0x1xx 帧）；F103 动力域为 **0x0B**
- 显示域 100 ms 内未收到 0x180 → 标记 CAN 离线，UI 提示
- 动力域 200 ms 内未收到 0x200 → 自动停机，进入安全状态

### 8.2 报文 ID 分配

| ID (Hex) | 名称 | 方向 | 周期 | DLC |
|----------|------|------|------|-----|
| 0x180 | Motor_Status | F103 → F407 | 20 ms | 8 |
| 0x181 | Motor_Diag | F103 → F407 | 100 ms | 8 |
| 0x200 | Motor_Command | F407 → F103 | 50 ms | 8 |
| 0x201 | Display_Info | F407 → F103 | 200 ms | 8 |
| 0x700 | Bootloader_Notify | F103 → F407 | 1000 ms | 2 |

### 8.3 详细字段

#### 0x180 Motor_Status (F103 → F407, 20 ms)

| Byte | 内容 | 说明 |
|------|------|------|
| 0–1 | actual_rpm | uint16_le，单位 rpm |
| 2–3 | target_rpm | uint16_le，单位 rpm |
| 4 | pwm_percent | 0–100 |
| 5 | motor_state | 0 stop, 1 run, 2 fault |
| 6 | fault_code | 0=无故障 |
| 7 | alive_counter | 0–255 循环 |

#### 0x181 Motor_Diag (F103 → F407, 100 ms)

| Byte | 内容 | 说明 |
|------|------|------|
| 0 | temperature | 0xFF 表示未接 |
| 1 | voltage_x10 | 单位 0.1V |
| 2 | current_x10 | 单位 0.1A |
| 3 | control_mode | 0 open-loop, 1 speed-loop |
| 4–7 | reserved | |

#### 0x200 Motor_Command (F407 → F103, 50 ms)

| Byte | 内容 | 说明 |
|------|------|------|
| 0 | enable | 0 stop, 1 run |
| 1 | mode | 0 open-loop, 1 speed-loop |
| 2–3 | target_rpm | uint16_le |
| 4 | target_pwm | 0–100 (开环用) |
| 5 | clear_fault | 1=清故障 |
| 6 | command_counter | 0–255 循环 |
| 7 | checksum | XOR(0–6) |

#### 0x201 Display_Info (F407 → F103, 200 ms)

| Byte | 内容 | 说明 |
|------|------|------|
| 0 | display_state | 0=正常 1=OTA 中 2=故障 |
| 1 | app_version_major | |
| 2 | app_version_minor | |
| 3 | app_version_patch | |
| 4–7 | reserved | |

#### 0x700 Bootloader_Notify (F103 → F407, 1000 ms)

| Byte | 内容 | 说明 |
|------|------|------|
| 0 | bootloader_present | 1=进入 Bootloader |
| 1 | version_major | |

> F103 暂不实现 Bootloader，但保留 ID 以备扩展。

### 8.4 CAN 过滤器配置（关键）

STM32 bxCAN 必须配置过滤器才能接收报文，否则所有帧都会被丢弃。

**F407 显示域（接收 F103 的 0x180/0x181/0x700）：**

```c
// CAN_FilterConfTypeDef 配置示例
// 使用 32 位列表模式，过滤出动力域报文
CAN_FilterTypeDef filter;
filter.FilterBank = 0;
filter.FilterMode = CAN_FILTERMODE_IDLIST;    // 列表模式
filter.FilterScale = CAN_FILTERSCALE_32BIT;   // 32 位
filter.FilterIdHigh = 0x180 << 5;             // ID 左移 5 位对齐
filter.FilterIdLow  = 0;
filter.FilterMaskIdHigh = 0x181 << 5;
filter.FilterMaskIdLow  = 0;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterActivation = ENABLE;
filter.SlaveStartFilterBank = 14;             // CAN1 用 0-13，CAN2 用 14-27
HAL_CAN_ConfigFilter(&hcan1, &filter);

// 第二个过滤器组接收 0x700
filter.FilterBank = 1;
filter.FilterIdHigh = 0x700 << 5;
filter.FilterMaskIdHigh = 0x700 << 5;
HAL_CAN_ConfigFilter(&hcan1, &filter);
```

**F103 动力域（接收 F407 的 0x200/0x201）：**

```c
// F103 只有 CAN1，过滤器组 0-13
CAN_FilterTypeDef filter;
filter.FilterBank = 0;
filter.FilterMode = CAN_FILTERMODE_IDLIST;
filter.FilterScale = CAN_FILTERSCALE_32BIT;
filter.FilterIdHigh = 0x200 << 5;
filter.FilterMaskIdHigh = 0x201 << 5;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterActivation = ENABLE;
HAL_CAN_ConfigFilter(&hcan1, &filter);
```

> **注意**：F407 有 CAN1（主）和 CAN2（从），CAN2 的过滤器从 `SlaveStartFilterBank` 开始编号。本项目只用 CAN1，可设 `SlaveStartFilterBank=14`。
> 如果调试时收不到报文，第一件事检查过滤器配置。

---

## 9. 软件模块拆分

### 9.0 仓库根目录结构

```
Car_Panel/                          # Git 仓库根目录
├── Car_Panel_Project_Plan.md       # 本文档
├── CLAUDE.md                       # AI 辅助开发配置
├── .gitignore
├── README.md
├── docs/                           # 项目文档
│   ├── schematic/                  # 原理图 PDF
│   └── notes/                      # 开发笔记
├── display_ecu_f407/               # 显示域工程（见 9.1）
│   ├── bootloader/
│   ├── app/
│   ├── common/
│   └── tools/
├── power_ecu_f103/                 # 动力域工程（见 9.2）
└── tools/                          # 全局工具脚本
    ├── ymodem_send.py
    └── can_test.py
```

> **分支策略**：`main`（稳定发布）← `dev`（日常集成）← `feature/*`（特性开发）。

### 9.1 显示域 F407 工程结构

```
display_ecu_f407/
├── bootloader/                # Bootloader 工程
│   ├── Core/
│   │   ├── Src/
│   │   │   ├── main.c
│   │   │   ├── bsp_can.c
│   │   │   ├── bsp_flash.c      # 片内 Flash 读写
│   │   │   ├── bsp_w25q64.c     # SPI3 外置 Flash
│   │   │   ├── bsp_at24c02.c    # I2C3 EEPROM
│   │   │   ├── bsp_crc32.c      # CRC32 校验
│   │   │   ├── bsp_log.c        # 串口日志
│   │   │   └── bsp_uart.c       # YMODEM 接收
│   │   └── Inc/
│   ├── MDK-ARM/                # Keil 工程
│   └── README.md
│
├── app/                       # LVGL App 工程
│   ├── Core/
│   │   ├── Src/
│   │   │   ├── main.c
│   │   │   ├── freertos.c       # FreeRTOS 配置
│   │   │   ├── bsp_can.c
│   │   │   ├── bsp_lcd.c        # FSMC 8080
│   │   │   ├── bsp_touch.c      # XPT2046
│   │   │   ├── bsp_w25q16.c     # 板载 Flash (字库)
│   │   │   ├── bsp_w25q64.c     # 外置 Flash (图片)
│   │   │   ├── bsp_key.c
│   │   │   ├── bsp_led.c
│   │   │   ├── bsp_uart.c
│   │   │   ├── bsp_log.c
│   │   │   ├── can_app.c        # CAN 协议解析
│   │   │   ├── lvgl_port.c      # LVGL 移植
│   │   │   └── ui/
│   │   │       ├── ui_main.c    # 主仪表页
│   │   │       ├── ui_diag.c    # 诊断页
│   │   │       ├── ui_ota.c     # OTA 页
│   │   │       └── ui_assets.c  # 资源加载
│   │   └── Inc/
│   ├── lvgl/                   # LVGL v8 源码
│   ├── MDK-ARM/
│   └── README.md
│
├── common/                    # Bootloader 与 App 共享
│   ├── protocol/
│   │   ├── can_ids.h
│   │   ├── can_msg.h
│   │   └── can_msg.c
│   ├── bsp/
│   │   ├── bsp_can.h
│   │   ├── bsp_flash.h
│   │   ├── bsp_w25q64.h
│   │   └── bsp_at24c02.h
│   └── linker/
│       └── Car_Panel.sct       # 共享的分散加载文件
│
└── tools/
    ├── gen_app_bin.py          # 把 .axf 转 .bin 的脚本
    ├── ymodem_send.py          # 串口 YMODEM 发送工具
    └── can_test.py             # PC 端 CAN 模拟工具（用 USB-CAN 模块时）
```

### 9.2 动力域 F103 工程结构

```
power_ecu_f103/
├── Core/
│   ├── Src/
│   │   ├── main.c
│   │   ├── stm32f1xx_it.c
│   │   ├── bsp_can.c
│   │   ├── bsp_pwm.c           # TIM1_CH1 输出
│   │   ├── bsp_encoder.c       # TIM2 编码器
│   │   ├── bsp_key.c
│   │   ├── bsp_led.c
│   │   ├── bsp_uart.c
│   │   ├── bsp_log.c
│   │   ├── motor.c             # 电机控制逻辑
│   │   ├── pid.c               # PID 控制器
│   │   ├── can_app.c
│   │   └── fault.c             # 故障检测
│   └── Inc/
├── MDK-ARM/
└── README.md
```

### 9.3 FreeRTOS 任务规划（F407 App）

> **栈大小单位为字（Word = 4 Byte）**。例如 512 words = 2048 bytes。

| 任务 | 优先级 | 栈大小 (words) | 实际栈 (bytes) | 周期 | 说明 |
|------|-------|---------------|---------------|------|------|
| CAN_Rx | 5 | 512 | 2048 | 事件驱动 | CAN 接收回调后处理 |
| LVGL_Tick | 3 | 256 | 1024 | 5 ms | LVGL 心跳 |
| LVGL_Task | 2 | 2048 | 8192 | 事件驱动 | LVGL 主循环 |
| UI_Update | 3 | 1024 | 4096 | 50 ms | 刷新仪表数据 |
| Key_Scan | 4 | 256 | 1024 | 20 ms | 按键扫描与去抖 |
| Log_Task | 1 | 512 | 2048 | 100 ms | 异步日志输出 |
| Watchdog | 6 | 128 | 512 | 1000 ms | 喂狗（最高优先级） |
| Idle | 0 | 256 | 1024 | — | 系统空闲钩子 |

> **FreeRTOS 堆管理策略**：推荐使用 `heap_4.c`（支持碎片合并），总堆大小建议 30–40 KB。
> **LVGL 内存**：从 FreeRTOS 堆中分配 `lv_mem`，或使用独立 CCM 内存区域（F407 有 64 KB CCM RAM，地址 0x10000000，DMA 不可访问，适合 LVGL）。

> F103 任务简单，**裸机 + 状态机**即可，不上 RTOS。

### 9.4 串口日志规范（统一 `bsp_log.h`）

```c
// bsp_log.h
#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3

#define LOG_TAG_CAN      "[CAN]   "
#define LOG_TAG_MOTOR    "[MOTOR] "
#define LOG_TAG_UI       "[UI]    "
#define LOG_TAG_OTA      "[OTA]   "

#define LOG_E(tag, fmt, ...) printf("[E]" tag fmt "\r\n", ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) printf("[W]" tag fmt "\r\n", ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) printf("[I]" tag fmt "\r\n", ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) printf("[D]" tag fmt "\r\n", ##__VA_ARGS__)

// 用法
LOG_I(LOG_TAG_CAN, "Motor_Status rx: rpm=%d pwm=%d%%", rpm, pwm);
```

---

## 10. OTA/IAP 实现路线

### 10.1 第一版：串口/YMODEM + W25Q64 + Bootloader

**流程**：

1. PC 用 `ymodem_send.py` 通过串口发送 `display_app.bin`（约 400 KB）
2. F407 App 处于 OTA 模式（用户长按 KEY1 进入）
3. App 通过 YMODEM 接收 bin，按 4 KB 块写入 W25Q64 暂存区
4. 全部写入后计算 CRC32，写入 AT24C02 升级标志（pending_version, pending_crc32, pending_size, upgrade_request）
5. NVIC_SystemReset() → 复位
6. Bootloader 启动，读 AT24C02
7. 若 upgrade_request=1，从 W25Q64 读固件，校验 CRC32
8. 校验通过 → 擦除 App 区 → 写入新固件 → 置 last_upgrade_result=1 → 跳 App
9. 校验失败 → 置 last_upgrade_result=2，回滚或停机

**关键点**：
- 升级失败绝不破坏旧 App（用 A/B 双分区或单分区 + 失败回退）
- AT24C02 magic 必须正确写入，否则 Bootloader 不认
- 看门狗在 Bootloader 中要持续喂狗，**别让它复位 Bootloader 自己**
- CRC32 用查表法，Bootloader 启动后立即打开 IWDG
- **VTOR 重定位**（必须）：App 从非零地址（0x08020000）启动时，必须在 `SystemInit()` 或 `main()` 最开头设置中断向量表基址：

```c
// App 工程的 system_stm32f4xx.c 或 main.c 中
#define VECT_TAB_OFFSET  0x00020000   // App 偏移 128 KB
// 在 SystemInit() 中自动设置：
// SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
// 即 SCB->VTOR = 0x08000000 | 0x00020000 = 0x08020000
```

  如果不设置 VTOR，所有中断（含 PendSV、SysTick）都会跳到 Bootloader 的向量表，导致 HardFault。
- **Bootloader 跳转 App 的标准流程**：

```c
typedef void (*pFunction)(void);
void jump_to_app(uint32_t app_addr) {
    uint32_t app_stack = *(__IO uint32_t*)app_addr;
    uint32_t app_entry = *(__IO uint32_t*)(app_addr + 4);

    // 1. 检查栈指针合法性（应在 SRAM 范围内）
    if ((app_stack & 0x2FFE0000) != 0x20000000) {
        LOG_E(LOG_TAG_OTA, "Invalid app stack: 0x%08X", app_stack);
        return;
    }

    // 2. 关闭所有中断，清除所有挂起标志
    __disable_irq();
    HAL_DeInit();  // 反初始化所有 HAL 外设

    // 3. 设置 VTOR 并跳转
    SCB->VTOR = app_addr;
    __set_MSP(app_stack);
    pFunction jump = (pFunction)app_entry;
    jump();
}
```

### 10.2 第二版：App 接收 + Bootloader 搬运

App 接收固件（串口 / USB CDC / SD 卡 / ESP32 网关）→ 写入 W25Q64 → 写升级标志 → 复位。

升级流程用 LVGL 显示进度条、校验结果、错误码，用户体验好。

### 10.3 第三版：ESP32-DOWN-V3 WiFi 网关

ESP32 连接 WiFi，提供 HTTP 网页上传固件；通过 UART 把固件分包发给 F407；F407 写入 W25Q64 → 升级。

---

## 11. LVGL 仪表盘页面规划

### 11.1 页面结构

```
[主仪表页]  ←KEY0→  [诊断页]  ←KEY0→  [OTA 页]
   ↑                                          │
   └─────KEY1 短按 启停 / KEY1 长按 → OTA ←──┘
```

### 11.2 主仪表页

**布局**（480×320）：

```
+------------------------------------------+
| [状态栏]  ●CAN   速度:120rpm  v1.0.0     |  20 px
+------------------------------------------+
|                                          |
|         120                              |
|         rpm                              |  数字显示 100 px
|                                          |
|    ╭─────────╮                           |
|    │  仪表   │   [目标: 150 rpm]         |  圆弧仪表 200 px
|    ╰─────────╯                           |
|                                          |
| [启停] [故障:无] [OTA:就绪]              |  状态按钮 50 px
+------------------------------------------+
```

### 11.3 诊断页

显示：
- 实际 / 目标 RPM
- PWM 占空比
- CAN alive counter
- 最近 5 次 fault_code
- 报文时间戳

### 11.4 OTA 页

显示：
- 当前 App 版本（从 AT24C02 读）
- W25Q64 暂存区检测到的固件版本
- 接收进度条
- CRC32 校验结果
- 升级成功/失败状态
- 触发升级按钮

### 11.5 资源加载策略

- **ASCII 字体 + 数字**（约 30 KB）：片上 Flash 编译进 App
- **小图标 + UI 元素**（约 100 KB）：从板载 W25Q16 加载
- **启动画面 + 复杂图片**（约 200 KB）：从外置 W25Q64 加载
- **中文字库**（若需要，约 1–2 MB）：放 W25Q64，动态加载

---

## 12. 安全策略与故障处理

### 12.1 上电默认状态

- **电机默认停止**：`Motor_Command.enable=0` 直到显示域发送 1
- **F407 默认显示主仪表页**，不进入 OTA
- **F103 默认进入安全状态**，等 CAN 启动命令

### 12.2 故障码定义

| code | 名称 | 触发条件 | 处理 |
|------|------|---------|------|
| 0 | NONE | 无故障 | — |
| 1 | STALL | 堵转：PWM > 50% 且 RPM = 0 持续 500 ms | 停机，上报故障 |
| 2 | ENCODER_LOST | 编码器连续 100 ms 无脉冲 | 停机，上报故障 |
| 3 | CAN_TIMEOUT | 200 ms 未收到 0x200 | 停机，进入安全状态 |
| 4 | OVER_VOLTAGE | 电压 > 15V | 报警 |
| 5 | UNDER_VOLTAGE | 电压 < 9V | 报警 + 降功率 |
| 6 | OVER_CURRENT | 电流 > 5A 持续 200 ms | 停机 |
| 7 | IWDG_RESET | 看门狗复位 | 记录到日志 |

### 12.3 安全兜底

- **IWDG**：F407 和 F103 都开，超时 1 秒
- **软启动 ramp**：从 0% PWM 每 50 ms 增 5%，避免电流冲击
- **通信超时降级**：F103 端 200 ms 未收到 0x200 → 自动停机
- **OTA 失败不破坏**：升级失败保留旧 App，至少保证能再进 Bootloader
- **电源反接保护**：BTS7960 VM 入口串 10A 自恢复保险丝（F1）

### 12.4 急停

- **软件急停**：F407 LVGL 上"停止"按钮 / KEY1 短按
- **物理急停**（推荐加）：BTS7960 R_EN 拉低即可停机，可在 F103 端加一个自锁按键

---

## 13. 采购清单（修正版）

### 13.1 必买清单（第一版最小可用）

| 类别 | 型号 / 关键词 | 数量 | 单价区间 | 说明 |
|------|-------------|------|---------|------|
| 显示屏 | 3.5 寸 480×320 8080 并口 ILI9488/ILI9486 | 1 | 30–50 | 带触摸更好，不带也可 |
| CAN 收发器 | TJA1050 模块（5V） | 1 | 5–8 | 你已有 1 块，还需 1 块 |
| 备选 CAN 收发器 | SN65HVD230 模块（3.3V） | 1 | 5–8 | 二选一即可，3.3V 版本更稳 |
| 电机 | JGA25-370 12V AB 相霍尔编码器 | 1 | 30–50 | 第一版最容易成功 |
| 备选电机 | JGB37-520 12V AB 相霍尔编码器 | 1 | 40–70 | 想要更稳可选 |
| 电机驱动 | BTS7960 / IBT-2 H 桥模块 | 1 | 15–25 | 比 TB6612 耐用 |
| 12V 电源 | 12V/5A 开关电源适配器 | 1 | 25–40 | 5.5×2.1 DC 头 |
| 12V→5V DCDC | LM2596S 模块 / MP1584EN 模块 | 1 | 5–10 | 调到 5V 输出 |
| 终端电阻 | 120 Ω 1% 1/4W | 2 | < 1 | CAN 总线两端 |
| 杜邦线 | 母对母 + 母对公 | 各 40P | 5 | 跳线用 |
| 1000 μF/25V 电解 | 电机电源解耦 | 1 | 2 | BTS7960 VM 入口 |
| 自恢复保险丝 | 10A 30V | 1 | 3 | 电机电源防反接/短路 |

### 13.2 选买清单

| 类别 | 型号 | 说明 |
|------|------|------|
| USB-CAN 模块 | PEAK 或国产兼容 | PC 上位机调试用 |
| 逻辑分析仪 | 8 通道 ≥ 100 MHz | 调试 FSMC/SPI/Encoder 信号 |
| 万用表 / 示波器 | 基础即可 | 必带 |

### 13.3 已有清单（不需再买）

- STM32F407ZGT6 开发板 ×1
- STM32F103C8T6 核心板 ×1
- ESP32-DOWN-V3 ×1（暂不接）
- ESP32-C3 ×1（暂不接）
- 2208 无刷电机套件 ×1（第一版不用）
- AT24C02 EEPROM 模块 ×1
- W25Q64 SPI Flash 模块 ×1
- TJA1050 CAN 模块 ×1
- 调试 ST-Link / J-Link
- USB-TTL 串口模块

---

## 14. 开发阶段计划

### Phase 0：硬件采购与最小系统验证（1 周）

**目标**：所有硬件到齐，每个模块单独跑通。

- [ ] 12V 电源 + DCDC 5V 模块焊接验证，示波器确认纹波 < 100 mV
- [ ] F407 板上电，LED 闪，烧录 LED 闪烁程序
- [ ] F103 板上电，串口打印 "Hello"
- [ ] F407 FSMC 点亮 3.5 寸屏（先跑通纯白填充）
- [ ] F103 TIM1_CH1 输出 20 kHz PWM，示波器验证
- [ ] F103 TIM2 读取编码器，串口打印 RPM
- [ ] BTS7960 驱动电机，按方向开关验证正反转
- [ ] F407 与 F103 CAN 互联，TJA1050 接好 120 Ω，串口互发报文

**验收**：所有模块独立可用，控制台无错误日志。

### Phase 1：Bootloader + 最小 App（1 周）

**目标**：F407 有 Bootloader，能通过串口升级。

- [ ] 编写 Bootloader：检查 AT24C02 升级标志，从 W25Q64 读固件，CRC32 校验，写入 App 区，跳转
- [ ] 编写最小 App：LED 闪烁 + 串口打印 "Hello App"
- [ ] 串口发送 1 KB 测试 bin，验证升级流程
- [ ] OTA 失败回退验证（故意发错 CRC 的 bin）

**验收**：能通过串口升级；升级失败不破坏旧 App。

### Phase 2：动力域最小闭环（1 周）

**目标**：F103 独立完成电机开环调速 + 编码器测速。

- [ ] PWM 输出，串口命令调速
- [ ] 编码器测速，串口打印 RPM
- [ ] PID 参数初调（先 P，无 I 无 D）
- [ ] 软启动 ramp 实现
- [ ] 堵转/编码器丢失故障检测

**验收**：按键或串口命令控制电机，RPM 跟随目标值。

### Phase 3：CAN 通信闭环（1 周）

**目标**：F103 与 F407 通过 CAN 交换数据。

- [ ] F103 周期发 0x180，调试串口打印
- [ ] F407 接收 0x180，更新本地变量
- [ ] F407 周期发 0x200，F103 收到后启停电机
- [ ] 通信超时检测（断线/重连）

**验收**：F407 能控制 F103 电机启停；断线时双方正确进入安全状态。

### Phase 4：LVGL 仪表盘（2 周）

**目标**：F407 显示完整 UI。

- [ ] 移植 LVGL v8
- [ ] FSMC 8080 驱动调试（先静态显示一帧图片）
- [ ] 主仪表页：数字 + 圆弧仪表
- [ ] 诊断页：实时数据
- [ ] OTA 页：进度条 + CRC
- [ ] 按键切换页面
- [ ] FreeRTOS 任务调度验证（CPU 占用率）

**验收**：UI 流畅 30 fps；CAN 数据驱动 UI 实时刷新。

### Phase 5：OTA 完整流程（1 周）

**目标**：端到端升级可用。

- [ ] YMODEM 协议实现
- [ ] 串口升级工具脚本
- [ ] LVGL 升级进度显示
- [ ] 多版本管理
- [ ] 异常处理（断电、断线、错包）

**验收**：完整跑通"接收 → 校验 → 升级 → 启动"。

### Phase 6：闭环控制 + 诊断（1 周）

**目标**：更像真实 ECU。

- [ ] PID 完整调参（先用 Ziegler-Nichols 法）
- [ ] 故障码表完善
- [ ] 显示域故障显示
- [ ] CAN 报文故障码字段
- [ ] 升级日志写到 W25Q16

**验收**：故障触发后能定位原因；PID 阶跃响应超调 < 10%。

### Phase 7：ESP32-DOWN-V3 WiFi 网关（1–2 周，可选）

**目标**：远程 OTA。

- [ ] ESP32 连接 WiFi
- [ ] HTTP 网页上传固件
- [ ] ESP32 与 F407 UART 传输固件
- [ ] F407 升级流程接入

**验收**：手机/PC 通过网页触发升级。

---

## 15. 下一步行动清单

按以下顺序执行：

1. **采购**：根据 13.1 必买清单下单，预计 1 周内到齐
2. **建仓**：在 Car_Panel 目录初始化 git 仓库，分支策略：
   - `main`：稳定版本
   - `dev`：日常开发
   - `feature/*`：特性分支
   - `release/*`：发布前
3. **建工程**：创建 `display_ecu_f407/bootloader`、`display_ecu_f407/app`、`power_ecu_f103` 三个 Keil 工程
4. **配置 CubeMX**：用 STM32CubeMX 生成初始化代码，**不要手写寄存器**
5. **跑通 Phase 0**：硬件最小验证
6. **进入 Phase 1**：开始 Bootloader
7. **后续按 Phase 顺序推进**

> 建议：任何阶段遇到问题，先**缩小范围隔离**（单模块验证），再回到系统联调。

---

## 16. 调试策略

### 16.1 调试工具与环境

| 工具 | 用途 | 备注 |
|------|------|------|
| ST-Link V2 / J-Link | SWD 在线调试、断点、变量观察 | F407 和 F103 均支持 |
| USB-TTL (CH340/CP2102) | 串口日志、YMODEM 固件传输 | F407 USART1 (PA9/PA10) |
| 逻辑分析仪 (8ch/100MHz) | 抓 FSMC/SPI/CAN/Encoder 波形 | 推荐买一块（约 50 元） |
| USB-CAN 适配器 | PC 端模拟 CAN 节点、抓包 | 选配，可用另一个 STM32 代替 |
| Keil MDK Debug | 断点调试、Watch 窗口、Logic Analyzer | 免费 License 限制 32 KB |
| STM32CubeMonitor | 实时变量可视化（无断点干扰） | 免费工具 |

### 16.2 分阶段调试要点

| 阶段 | 关键调试点 | 常见问题 |
|------|-----------|---------|
| Phase 0 | LED 闪烁、串口打印、PWM 波形 | 晶振不起振（检查 HSE 配置）、CAN 收发器方向接反 |
| Phase 1 | Bootloader 是否读到 AT24C02、跳转后 PC 是否正确 | VTOR 未设置 → HardFault；栈指针非法 → 死机 |
| Phase 2 | 编码器 CNT 是否变化、PWM 频率/占空比 | TIM2 编码器模式需 CH1+CH2 都接；BTS7960 使能脚 |
| Phase 3 | CAN 过滤器配置、报文 ID 是否正确 | 过滤器未配 → 收不到；波特率不匹配 → Bus-Off |
| Phase 4 | FSMC 时序、LVGL flush 函数、帧率 | 白屏 → FSMC 地址错；花屏 → 时序太块；卡顿 → 刷新策略 |
| Phase 5 | YMODEM 握手、W25Q64 写入校验 | 串口波特率不匹配丢包；W25Q64 写入前未擦除 |

### 16.3 HardFault 排查流程

```
HardFault 发生
  → 检查 SCB->HFSR（判断是精确/不精确/向量错误）
  → 检查 SCB->MMFAR / SCB->BFAR（出错的地址）
  → 查看 LR 寄存器（返回地址）判断出错位置
  → 常见原因：
    1. VTOR 未设置（App 从 0x08020000 启动但向量表还在 0x08000000）
    2. 空指针解引用（BSP 初始化前使用了未初始化的句柄）
    3. 栈溢出（FreeRTOS 任务栈不够，增大栈或减少局部变量）
    4. 除零（RPM 计算时编码器 PPR 为 0）
```

---

## 17. 测试计划

### 17.1 单元测试（模块级）

| 模块 | 测试方法 | 通过标准 |
|------|---------|---------|
| BSP_UART | 串口回环/打印 | 日志输出完整，无乱码 |
| BSP_CAN | 双板互发固定报文 | ID、数据、DLC 完全匹配 |
| BSP_W25Q64 | 写入→擦除→读回→比较 | 数据一致，JEDEC ID 正确 |
| BSP_AT24C02 | 写入→断电→读回 | 数据不丢失，页写正确 |
| BSP_PWM | 示波器验证频率和占空比 | 20 kHz ± 1%，占空比线性 |
| BSP_ENCODER | 手动转动电机→串口打印 RPM | 方向正确，转速合理 |
| PID | 阶跃响应测试 | 上升时间 < 500 ms，超调 < 10% |

### 17.2 集成测试（系统级）

| 测试场景 | 步骤 | 通过标准 |
|---------|------|---------|
| CAN 通信闭环 | F407 发启停命令 → F103 控制电机 → F407 显示 RPM | 延迟 < 100 ms，数据正确 |
| 通信故障 | 拔掉 CAN 线 | F407 100 ms 内显示"CAN 离线"；F103 200 ms 内自动停机 |
| OTA 升级 | 串口发送新固件 → Bootloader 校验写入 → App 正常启动 | 升级成功，App 版本号更新 |
| OTA 失败恢复 | 发送 CRC 错误的固件 | Bootloader 拒绝升级，旧 App 正常运行 |
| 电机堵转 | 手动卡住电机 | 500 ms 内检测到堵转，停机，上报 fault_code=1 |
| 急停 | 按下 F103 KEY_LOCAL 3 秒 | 电机立即停止，状态上报 F407 |
| 长时间运行 | 连续运行 2 小时 | 无看门狗复位，无内存泄漏，温度正常 |
| 电源波动 | 调节输入电压 9V–15V | 电机正常运转，无 MCU 复位 |

### 17.3 性能指标

| 指标 | 目标值 | 测量方法 |
|------|--------|---------|
| LCD 刷新帧率 | ≥ 25 fps | LVGL 内置 `lv_disp_get_refr_task` 计时 |
| CAN 报文延迟 | < 5 ms（端到端） | 示波器测 GPIO 翻转时间 |
| 电机转速控制精度 | 目标 ± 5% | 稳态 RPM 偏差 |
| Bootloader 升级时间 | < 30 s（768 KB 固件） | 计时器测量 |
| FreeRTOS CPU 占用率 | < 70% | `vTaskGetRunTimeStats()` |

---

## 18. 版本控制与仓库管理

### 18.1 分支策略

```
main (稳定版本，每个 Phase 完成后合并)
  └── dev (日常开发集成)
       ├── feature/bootloader     (Phase 1)
       ├── feature/motor-control  (Phase 2)
       ├── feature/can-comm       (Phase 3)
       ├── feature/lvgl-ui        (Phase 4)
       ├── feature/ota            (Phase 5)
       └── feature/pid-tuning     (Phase 6)
```

### 18.2 提交规范

```
<type>(<scope>): <subject>

type: feat | fix | docs | refactor | test | chore
scope: bootloader | app | motor | can | lvgl | ota | bsp

示例：
  feat(can): 实现 CAN 过滤器配置和报文收发
  fix(motor): 修复编码器方向判断逻辑
  docs: 更新项目计划文档 v2.1
```

### 18.3 .gitignore 要点

```
# Keil MDK
*.uvguix.*
*.scvd
*.d
*.o
*.crf
*.htm
*.dep
*.lst
*.map
*.axf
*.htm
*.lnp
*.sct（保留自定义 scatter 文件，忽略生成的）
RTE/
Listings/
Objects/
DebugConfig/

# IDE
.vscode/
.idea/
*.swp

# 编译产物
*.bin
*.hex
*.elf

# 系统文件
Thumbs.db
.DS_Store

# CubeMX 生成但需保留的
# 保留 .ioc 文件，忽略不需要的生成目录
```

### 18.4 必须版本管理的文件

- `*.ioc`（STM32CubeMX 配置文件，可复现初始化代码）
- 自定义 scatter 文件 `*.sct`
- `Car_Panel_Project_Plan.md`
- 所有 `Core/Src/` 和 `Core/Inc/` 下的用户代码
- `lv_conf.h`（LVGL 配置）
- `FreeRTOSConfig.h`
- 工具脚本 `tools/*.py`
- 原理图 PDF

---

## 附录 A：版本与变更记录

| 版本 | 日期 | 变更 | 作者 |
|------|------|------|------|
| v1.0 | 2026-06-08 | 初版方案 | — |
| v2.0 | 2026-06-09 | 修正 Flash 分区、明确 F103C8T6、补充引脚规划、电源树、安全策略等 | — |
| v2.1 | 2026-06-09 | 修正 Scatter 文件示例、补充 CAN 过滤器配置、VTOR 重定位、FSMC 时序、FreeRTOS 堆管理、调试策略、测试计划、仓库管理规范 | — |

## 附录 B：参考文档

- STM32F407ZGT6 数据手册（RM0090）
- STM32F103C8T6 数据手册（RM0008）
- LVGL v8 官方文档：https://docs.lvgl.io/8.3/
- BTS7960 数据手册
- TJA1050 数据手册
- W25Q64 / AT24C02 数据手册
- 板上原理图：`STM32F407Z开发板--原理图-2019M(新液晶接口).PDF`
