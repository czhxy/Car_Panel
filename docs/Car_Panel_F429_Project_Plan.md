# Car_Panel 汽车双 ECU 仪表盘项目方案 —— STM32F429IGT6 显示域版本

> 项目代号：**Car_Panel-F429**
> 文档版本：**v3.1**（方案变更：LTDC/LED → SPI 2.8 寸触摸屏 MSP2834，移除 W25Q64/SDRAM/NAND）
> 目标：在 **STM32F429IGT6（显示域）+ STM32F103C8T6（动力域）+ CAN 总线** 架构上，实现真 AB 分区 OTA、LVGL 仪表盘 UI（SPI ILI9341V 屏 + FT6336G 电容触摸）、闭环电机控制。
> 本文档根据实际开发进度更新（FreeRTOS v11.3.0 + CAN 通信框架 + Bootloader 已完成）。

---

## 目录

1. [方案变更说明](#1-方案变更说明)
2. [硬件资源清单与角色分配](#2-硬件资源清单与角色分配)
3. [F429IGT6 引脚规划（显示域）](#3-f429igt6-引脚规划显示域)
4. [电源树设计](#4-电源树设计)
5. [Flash 与存储分区](#5-flash-与存储分区)
6. [主方案系统架构](#6-主方案系统架构)
7. [SPI LCD 驱动（ILI9341V）](#7-spi-lcd-驱动ili9341v)
8. [LVGL 移植与显示](#8-lvgl-移植与显示)
9. [OTA 升级方案（真 AB 分区）](#9-ota-升级方案真-ab-分区)
10. [CAN 通信协议](#10-can-通信协议)
11. [软件模块拆分](#11-软件模块拆分)
12. [开发阶段计划](#12-开发阶段计划)
13. [烧录与下载方式](#13-烧录与下载方式)
14. [调试策略](#14-调试策略)
15. [采购清单](#15-采购清单)

---

## 1. 方案变更说明

### 1.1 变更概述

原方案（v2.x）规划使用 LTDC 并行 RGB 接口驱动 TFT LCD + SDRAM 全屏双缓冲 + W25Q64 外挂 OTA。实际开发中：

| 变更项 | 原方案 (v2.x) | 现方案 (v3.1) | 原因 |
|--------|-------------|-------------|------|
| 显示接口 | LTDC RGB | **SPI2 4 线制** | 硬件方案变更，使用 MSP2834 SPI 屏 |
| LCD 控制器 | ILITEK RGB 565 | **ILI9341V (240×320)** | MSP2834 模块集成 |
| 触摸 | XPT2046 (SPI) | **FT6336G (I2C, 0x38)** | MSP2834 电容触摸 |
| SDRAM | 32MB W9825G6KH | **不使用** | SPI 屏自带 GRAM，无需 Framebuffer |
| DMA2D | 硬件加速 flush | **不使用** | SPI 逐行发送，DMA2D 无适用场景 |
| OTA 存储 | W25Q64 外挂 8MB | **内部 Flash 真 AB 分区** | 可直接写 App 槽区，无需暂存 |
| NAND | 128MB 预留 | **不使用** | 无大容量资源存储需求 |
| 已实现模块 | 零 | Bootloader + CAN + FreeRTOS 代码框架已完成，硬件联调未完成 | 实际开发进度 |

### 1.2 当前实际规格

| 项目 | 值 |
|------|-----|
| MCU | STM32F429IGT6, Cortex-M4 @ 180MHz |
| Flash | 2MB（项目使用前 1MB：Bootloader 64KB + OTA 参数 64KB + App A 384KB + App B 384KB + 预留 128KB） |
| RAM | 芯片物理资源为 192KB 主 SRAM + 64KB CCM；当前工程 scatter 使用 192KB 主 SRAM + 64KB CCM |
| RTOS | FreeRTOS v11.3.0 (heap_4, 64KB CCM 堆) |
| 显示屏 | MSP2834（2.8 寸 IPS, ILI9341V, 240×320 RGB565, SPI2） |
| 触摸 | FT6336G 电容触摸 (I2C1, 400kHz) |
| CAN | 500kbps, 29-bit 扩展帧, PA11/PA12 默认引脚 |
| Bootloader | YMODEM-1K OTA, 真 AB 分区, CRC32 校验, 零 Flash 搬运回滚 |

---

## 2. 硬件资源清单与角色分配

### 2.1 STM32F429IGT6 显示域

| 资源 | 型号 | 规格 | 当前使用状态 |
|------|------|------|-------------|
| 主频 | STM32F429IGT6 | 180 MHz | LQFP176，正常运行 |
| Flash | 片上 | 2MB（使用 1MB） | A/B 分区 OTA |
| RAM | 片上 | 192KB 主 SRAM + 64KB CCM | 当前工程映射 192KB 主 SRAM；CCM=FreeRTOS 堆，主 SRAM=DMA/全局 |
| SPI2 | 片上 | APB1 @ 45MHz | **MSP2834 LCD (ILI9341V)** |
| I2C1 | 片上 | APB1 @ 45MHz | **FT6336G 触摸 (400kHz)** |
| CAN1 | 片上 | 500kbps, PA11/PA12 默认引脚 | CAN 收发器 TJA1050 |
| USART1 | 片上 | 115200bps, PA9/PA10 | printf 日志 + 查询协议 |
| SDRAM | W9825G6KH | 32MB（板载） | 板载器件，当前软件不使用 |
| NAND | W29N01HV | 128MB（板载） | 板载器件，当前软件不使用 |
| 辅助 F103 | STM32F103CBT6 | LQFP48 | 板载 USB/UART 桥接（暂不使用） |
| CH340 | CH340N | USB UART | PA9(TX)/PA10(RX) |

**显示域角色**：运行 Bootloader + FreeRTOS + LVGL 仪表盘；通过 CAN 接收动力域数据并实时显示；通过 CAN 向动力域发送控制指令。

### 2.2 外置模块

| 模块 | 型号 | 接口 | 状态 |
|------|------|------|------|
| **SPI 触摸屏** | **MSP2834 (ILI9341V + FT6336G)** | SPI2 + I2C1 | 待开发 |
| CAN 收发器 | TJA1050 / SN65HVD230 | GPIO + CAN1 | 代码已接入，硬件未验证 |
| DRV8833 | 双 H 桥驱动器 ×2 | PWM + DIR + EN | 动力域，代码已实现，硬件未验证 |
| MG310 | 7.4V 直流减速电机 ×2，AB 相编码器 | Encoder 接口 | 动力域，代码已实现，硬件未验证 |
| F103C8T6 核心板 | 动力域 ECU | CAN + PWM | 现有 |

### 2.3 板载 LED 和按键

| 元件 | GPIO | 方向 | 已实现 |
|------|------|------|--------|
| LED1 | PH12 | 输出 | ✅ `bsp_led.c` |
| LED2 | PH10 | 输出 | ✅ |
| LED3 | PH11 | 输出 | ✅ |
| LED4 | PE3 | 输出 | ✅ |
| KEY1 | PE2 | 输入 | ✅ `bsp_key.c` (20ms 扫描) |
| KEY2 | PI11 | 输入 | ✅ |

---

## 3. F429IGT6 引脚规划（显示域）

### 3.1 引脚分配总表

| 模块 / 信号 | MCU 引脚 | 方向 | 备注 |
|-------------|---------|------|------|
| **电源 / 系统** | | | |
| 3V3 | VDD 多引脚 | - | 板载 LDO 输出 |
| GND | VSS 多引脚 | - | |
| VCAP_1 | Pin 81 | - | 必须接 2.2 µF 到地 |
| VCAP_2 | Pin 125 | - | 必须接 2.2 µF 到地 |
| NRST | Pin 31 | 输入 | 复位 |
| BOOT0 | Pin 166 | - | 跳线帽默认 0 |
| **SWD 调试** | | | |
| SWDIO | PA13 | 双向 | 调试下载 |
| SWCLK | PA14 | 输出 | 调试时钟 |
| **USART1 + CH340** | | | |
| USART1_TX | PA9 | 输出 | → CH340 RX |
| USART1_RX | PA10 | 输入 | ← CH340 TX |
| **CAN1 总线（默认引脚）** | | | |
| CAN1_RX | PA11 | 输入 | 默认引脚 |
| CAN1_TX | PA12 | 输出 | 默认引脚 |
| **SPI2 LCD (MSP2834 ILI9341V)** | | | |
| SPI2_SCK | **PB13** | OUT, AF5 | 串行时钟 |
| SPI2_MOSI | **PB15** | OUT, AF5 | 主出从入 |
| SPI2_MISO | **PB14** | IN, AF5 | 可选回读 |
| LCD_CS | **PB12** | OUT | 片选（软件 NSS, 低有效） |
| LCD_DC | **PB10** | OUT | 数据/命令选择（高=数据） |
| LCD_RST | **PB11** | OUT | LCD 复位（低有效） |
| LCD_BL | **PB0** | OUT | 背光控制（高=ON） |
| **I2C1 触摸 (FT6336G)** | | | |
| I2C1_SCL | **PB6** | OUT, AF4 | 400kHz 时钟 |
| I2C1_SDA | **PB7** | BIDIR, AF4 | 开漏，模块自带上拉 |
| CTP_INT | **PB8** | IN, EXTI8 | 触摸中断（低有效） |
| CTP_RST | **PB9** | OUT | 触摸复位（低有效） |

> PB8/PB9 当前分别用于 CTP_INT 和 CTP_RST；CAN1 使用 PA11/PA12 默认引脚，因此此处不存在 CAN/I2C 引脚冲突。

### 3.2 实际使用的 LED 和按键

| 元件 | GPIO | 方向 | 已实现 |
|------|------|------|--------|
| LED1 | PH12 | OUT | ✅ 心跳指示 |
| LED2 | PH10 | OUT | ✅ |
| LED3 | PH11 | OUT | ✅ |
| LED4 | PE3 | OUT | ✅ |
| KEY1 | PE2 | IN | ✅ CAN 测试发送 |
| KEY2 | PI11 | IN | ✅ 预留 |

### 3.3 引脚冲突说明

由于方案从 LTDC + SDRAM + NAND + W25Q64 改为 SPI2 + I2C1，所有新增引脚均位于 GPIOB；CAN1 使用 PA11/PA12 默认引脚，当前无已确认的 CAN/I2C 冲突。PA11/PA12 同时连接板载 USB FS，但当前软件不使用 USB FS。板载 SDRAM/NAND 不由当前软件使用，FMC 引脚保持默认状态。

---

## 4. 电源树设计

### 4.1 板载电源架构

```
12V DC Input
    ↓
U11 (MT9700) → +5V
    ↓
U13 (AMS1117-3.3) → +3.3V → F429, 板载存储器, CH340
    ↓
F429 VDDA → 模拟电源（磁珠隔离）
```

### 4.2 F429IGT6 供电要求

| 引脚 | 接法 | 说明 |
|------|------|------|
| VDD/VSS | 多对 | 每对就近放 100 nF 退耦 |
| VDDA/VSSA | 3.3V/地 | ADC 模拟电源，串磁珠 |
| VREF+ | 与 VDDA 同源 | 按 STM32F429 数据手册连接 |
| **VCAP_1** | **2.2 µF → 地** | 内部稳压输出，必须接 |
| **VCAP_2** | **2.2 µF → 地** | 必须接 |
| VBAT | 3.3V 或纽扣电池 | 后备域 |

### 4.3 扩展模块供电

| 模块 | 电压 | 来源 |
|------|------|------|
| MSP2834 | 推荐 5V | 模块板载 3.3V 稳压，按用户手册供电 |
| W25Q64 模块 | 3.3V | 当前方案不使用 |
| CAN 收发器 | 3.3V 或 5V | 外部 LDO |

---

## 5. Flash 与存储分区

### 5.1 F429IGT6 片上 Flash 分区（使用 1MB / 总量 2MB）

```
0x08000000 - 0x0800FFFF   Bootloader       64 KB   Sector 0-3
0x08010000 - 0x0801FFFF   OTA 参数区       64 KB   Sector 4 (append-only 日志)
0x08020000 - 0x0807FFFF   App A             384 KB  Sector 5-7
0x08080000 - 0x080DFFFF   App B             384 KB  Sector 8-10
0x080E0000 - 0x080FFFFF   (预留)           128 KB  Sector 11
```

> 真 AB 分区机制：每个 App 槽独立链接（通过 scatter 文件），Bootloader 根据 `active_partition` 设 `SCB->VTOR` 后跳转。升级时 YMODEM 写非活跃槽 → 翻转 active → 重启。回滚时免 Flash 搬运。

### 5.2 RAM 分区

```
0x20000000 - 0x2002FFFF   当前映射的主 SRAM 192KB    DMA 缓冲 / 全局变量 / 栈
0x10000000 - 0x1000FFFF   CCM 64KB         FreeRTOS 堆 (heap_4 ucHeap)
```

- LVGL 双缓冲（28.8KB）分配在主 SRAM（`0x2000xxxx`），CCM 无法被 DMA 访问
- FreeRTOS 堆位于 CCM，不与 DMA 缓冲竞争
- 主 SRAM 192KB 已全部纳入 scatter（`RW_IRAM1 0x20000000 0x00030000`），SRAM1/SRAM2/SRAM3 地址连续可直接使用

### 5.3 外部存储（不使用）

- **SDRAM** (W9825G6KH)：不使用。SPI 屏自带 GRAM，无需 Framebuffer。
- **NAND** (W29N01HV)：不使用。无大容量资源存储需求。
- **W25Q64**：不使用。OTA 采用内部 Flash 真 AB 分区，无需暂存。

---

## 6. 主方案系统架构

```text
                    CAN 总线 (500 kbps, 29-bit 扩展帧)
           CANH/CANL + 两端 120 Ω 终端
                        │
     ┌─────────────────┴─────────────────┐
     │                                   │
     ▼                                   ▼
+-----------+                       +-----------+
| 显示域 ECU|                       | 动力域 ECU|
| STM32F429 |                       | STM32F103 |
|           |                       |           │
| Bootloader|                       | 电机控制  |
| FreeRTOS |                       | PWM 驱动  |
| LVGL v8（规划） | <-------------------> | 编码器测速|
| SPI LCD   |                       | PID 闭环  |
| I2C Touch |                       | CAN 上报  |
| CAN 收发  |                       |           |
+-----+-----+                       +-----+-----+
      │                                   │
      │ SPI2 (ILI9341V)                  │ PWM/DIR/Encoder
      │ I2C1 (FT6336G)                   ▼
      ▼                           DC 减速电机 + H 桥
  MSP2834 2.8寸 SPI 屏             (DRV8833 ×2 + MG310 ×2)
  (240×320 RGB565, 电容触摸)
```

**FreeRTOS 任务架构（已实现 6 个任务）：**

| 任务 | 栈 | 优先级 | 周期 | 职责 |
|------|-----|--------|------|------|
| CAN_TX | 512 | 3 | 1ms | CAN 帧发送 + 电机控制帧 |
| CAN_RX | 512 | 3 | 事件 | CAN 帧接收 + 分发 |
| CAN_TEST | 256 | 3 | 事件 | 按键触发测试发送 |
| KEY_SCAN | 256 | 2 | 20ms | 按键扫描 |
| UART_QUERY | 256 | 2 | 事件 | 查询协议解析 |
| HEARTBEAT | 512 | 1 | 500ms | LED + CAN 心跳 |
| **DISPLAY** | **1024** | **2** | **5ms** | **LVGL tick + 渲染（待开发）** |

---

## 7. SPI LCD 驱动（ILI9341V）

### 7.1 硬件参数

| 参数 | 值 | 来源 |
|------|-----|------|
| 控制器 | ILI9341V | MSP2834 规格书 |
| 分辨率 | 240×320 | 同上 |
| 颜色格式 | RGB565 (16bit) | 同上 |
| SPI 模式 | CPHA=0, CPOL=0 (模式 0) | ILI9341 数据手册 |
| 命令/数据 | DC 引脚：低=命令, 高=数据 | 同上 |
| 背光控制 | BL 引脚高=ON（N-MOSFET 驱动） | MSP2834 用户手册 |
| 复位时序 | RST 低>100ms → 高 | ILI9341V_Init.txt |

### 7.2 SPI2 初始化

```c
// SPI2: PB13=SCK, PB14=MISO, PB15=MOSI (AF5)
// 5.625MHz (APB1 45MHz / 8), 8bit, Mode 0, MSB First
SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  // 5.625 MHz
SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;    // CPHA=0
SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;       // CPOL=0
```

### 7.3 关键命令原语

| 函数 | 说明 |
|------|------|
| `BSP_LCD_WriteCmd(uint8_t cmd)` | CS=0, DC=0, SPI 发送 1 字节命令 |
| `BSP_LCD_WriteData(uint8_t data)` | CS=0, DC=1, SPI 发送 1 字节数据 |
| `BSP_LCD_WriteData16(uint16_t data)` | DC=1, SPI 发送 2 字节 RGB565 |
| `BSP_LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)` | 写 0x2A + 0x2B 设置绘制窗口 |

### 7.4 初始化序列

完整序列见 `docs/schematic/MSP2834/MSP2834/ILI9341V_Init.txt`，核心步骤：

1. 硬件复位 → 电源配置 (0xCF/0xED/0xE8/0xCB/0xF7/0xEA)
2. 电源控制 (0xC0/0xC1/0xC5/0xC7) + Display Inversion (0x21)
3. Memory Access Control (0x36=0x08, BGR顺序)
4. Pixel Format (0x3A=0x55, 16bit RGB565)
5. Frame Rate (0xB1) + Gamma (0xE0/0xE1)
6. Sleep Out (0x11) → 120ms delay → Display ON (0x29)

---

## 8. LVGL 移植与显示

### 8.1 配置参数

```c
// lv_conf.h
#define LV_COLOR_DEPTH            16      // RGB565
#define LV_HOR_RES_MAX            240
#define LV_VER_RES_MAX            320
#define LV_DISP_DEF_REFR_PERIOD   5       // 5ms 刷新周期
#define LV_MEM_SIZE               (32 * 1024)  // 规划值；LVGL 尚未加入当前工程
```

### 8.2 双缓冲方案

```
双缓冲: 2 × (240 × 30 像素) × 2 byte(RGB565) = 28.8 KB
位置: 主 SRAM (0x20000000), 全局静态分配 (__attribute__((section(".bss"))))
```

SPI 屏自带 GRAM，无需 SDRAM Framebuffer。LVGL 绘制到缓冲 → flush_cb 逐行 SPI 发送到 ILI9341 GRAM。

### 8.3 flush_cb 实现

```c
void lcd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    BSP_LCD_SetWindow(area->x1, area->y1, area->x2, area->y2);
    BSP_LCD_WriteCmd(0x2C);  // Memory Write

    for (uint32_t i = 0; i < w * h; i++) {
        BSP_LCD_WriteData16(color_p[i].full);
    }

    lv_disp_flush_ready(drv);
}
```

### 8.4 触摸 indev 回调

```c
void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x, y;
    if (FT6336G_ReadTouch(&x, &y)) {      // 读取 0x02-0x06 寄存器
        data->point.x = x;                 // X: (P1_XH[3:0]<<8)|P1_XL
        data->point.y = y;                 // Y: (P1_YH[3:0]<<8)|P1_YL
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state   = LV_INDEV_STATE_RELEASED;
    }
}
```

### 8.5 显示任务

```c
void Task_Display(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        lv_tick_inc(5);
        lv_timer_handler();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5));
    }
}
```

---

## 9. OTA 升级方案（真 AB 分区）

### 9.1 方案概述

OTA 使用**内部 Flash 真 AB 分区**，**不依赖外挂 Flash**。

```
升级流程:
  Bootloader 接收 YMODEM 固件 → 直接写入非活跃槽位 Flash
  → 校验 reset-handler 地址 → 翻转 active_partition → 重启

回滚机制:
  Bootloader 检查 CRC32 → 连续启动失败达到阈值后切换活跃槽位标记
  → 零 Flash 搬运，直接跳转备用槽位
```

### 9.2 分区详情

参见 §5.1。每个 App 槽独立链接（通过 scatter 文件 `app.sct`/`app_b.sct`）：
- App A 链接到 `0x08020000`
- App B 链接到 `0x08080000`

### 9.3 OTA 参数区（Sector 4）

- Append-only 磨损均衡日志，1024 槽 × 64B
- 每条记录含 CRC32，掉电安全
- `boot_decision.c` 负责 CRC 校验 + 选址 + 回滚决策

### 9.4 关键设计原则

- YMODEM 接收固件直接写入目标 Flash 槽位（非 W25Q64 暂存）
- 升级前校验 reset-handler 合法地址（防错包写入后跳转 HardFault）
- 回滚免 Flash 搬运（直接切换活跃标记）

---

## 10. CAN 通信协议

> 当前项目使用 29-bit 扩展帧协议，定义在 `project/display_ecu_f429/protocol/CAN_Protocol.h` 和 `project/power_ecu_f103/protocol/CAN_Protocol.h`。ID 编解码和基础收发已实现，但心跳 mode 和电机状态帧解析仍需对齐。

### 10.1 基础参数

| 参数 | 值 |
|------|-----|
| 波特率 | 500 kbps (Prescaler=9, BS1=7tq, BS2=2tq @ 45MHz APB1) |
| 帧格式 | 29-bit 扩展帧 |
| 本机地址 | CAN_ADDR_MAINBOARD (0x01) |
| 对端地址 | CAN_ADDR_MOTORBOARD (0x02) |
| 终端电阻 | 120 Ω × 2 (总线两端) |

### 10.2 CAN ID 编码（29-bit 扩展帧）

```
[28:26] prio   [25:22] src   [21:18] dst   [17:16] ftype   [15:6] mode   [5:0] func
```

使用宏 `CAN_ID_BUILD(prio, src, dst, ftype, mode, func)` 或函数 `CanProto_EncodeId()` 构造 ID。

### 10.3 当前已实现的帧

| 帧 | 周期 | 方向 | 说明 |
|----|------|------|------|
| 心跳帧 | 500ms | F429 → 广播 | 当前 F429 代码使用 mode=0x000；F103 已按 mode=0x320 发送，协议待统一 |
| 电机控制帧 | 10ms 限频 | F429 → F103 | `MODE_ID_CTRL_LF` 0x020，速度/角度；当前传感器接口返回 0，且只发送左电机控制帧 |
| 电机状态帧 | 10ms | F103 → F429 | `MODE_ID_STATUS_MOTOR` 0x110，左右电机各一帧；F429 当前尚未解析 |
| 测试帧 | 按键触发 | F429 → 广播 | 8 字节递增测试数据 |

### 10.4 CAN 通信架构

```
TX: 应用层 → ModCanFrame → Mod_Can_TxEvent() → CanTxQueue (FIFO 64)
      → ModCommCan_Tx() → CanTxMsg → CAN_Transmit() → 硬件邮箱
      邮箱满 → xQueueSendToFront 回灌队首，break

RX: CAN FIFO0 ISR → Mod_Can_RxIRQHandler() → CanRxQueue (FIFO 64)
      → Mod_Can_RxTask() → ModCommCan_OnRxFrame() (弱符号)
```

---

## 11. 软件模块拆分

### 11.1 实际目录结构

```
project/display_ecu_f429/
├── app/                  BSP 驱动层
│   ├── bsp_can.c/h       ✅ CAN 初始化 + 滤波器
│   ├── bsp_led.c/h       ✅ LED 驱动 (4 路)
│   ├── bsp_key.c/h       ✅ 按键扫描 (20ms, FreeRTOS 信号量)
│   ├── bsp_log.h         ✅ 日志宏 (LOG_E/W/I/D → printf)
│   └── main.c/h          ✅ App 入口
├── bootloader/           ✅ YMODEM OTA + 真 AB 分区
│   ├── boot_decision.c   ✅ CRC32 校验 + 选址 + 回滚
│   └── ota_params.c      ✅ Sector 4 append-only 日志
├── components/           目前为备用环形队列
│   └── my_queue.c/h
├── driver/               底层驱动
│   ├── Delay.c/h         (App 内应使用 vTaskDelay)
│   ├── usart.c/h         ✅ printf 重定向 + 环形缓冲 RX
│   └── mod_motor.c/h     占位 (返回 0.0f)
├── firmware/             CMSIS + STM32F4xx SPL
├── protocol/             CAN 协议定义
│   └── CAN_Protocol.h    ✅ 29-bit ID 编解码
├── task/                 FreeRTOS 任务
│   ├── task_entry.c      ✅ 初始化入口 + 任务创建
│   ├── mod_comm_can.c/h  ✅ CAN TX/RX 框架
│   ├── task_comm_can_protocol.c/h ✅ CAN 协议层
│   └── task_query.c/h    ✅ UART 查询服务
├── mdk/                  Keil 工程 + scatter 文件
└── third_lib/            FreeRTOS v11.3.0
```

> SPI LCD、FT6336G 触摸、LVGL 移植和显示任务均尚未创建，规划文件位于
> `project/display_ecu_f429/docs/spi_touch_screen_plan.md`。

### 11.2 FreeRTOS 任务规划

| 任务 | 栈 | 优先级 | 周期 | 状态 |
|------|-----|--------|------|------|
| ALL_Task_Entry | 256 | 30 | 一次性 | ✅ 初始化入口 |
| CAN_TX | 512 | 3 | 1ms | ✅ CAN 帧发送 |
| CAN_RX | 512 | 3 | 事件 | ✅ CAN 帧接收 |
| CAN_TEST | 256 | 3 | 事件 | ✅ 测试发送 |
| KEY_SCAN | 256 | 2 | 20ms | ✅ 按键扫描 |
| HEARTBEAT | 512 | 1 | 500ms | ✅ 心跳 |
| UART_QUERY | 256 | 2 | 事件 | ✅ 查询协议 |
| **DISPLAY** | **1024** | **2** | **5ms** | ⏳ LVGL 渲染（待开发）|

---

## 12. 开发阶段计划

| 阶段 | 内容 | 状态 |
|------|------|------|
| Phase 0 | 硬件验证 | ⏳ 未完成（硬件尚未全面验证） |
| Phase 1 | Bootloader + 真 AB 分区 | ✅ 完成 |
| Phase 2 | FreeRTOS + CAN 通信框架 | ✅ 完成 |
| Phase 3 | 动力域 + CAN 协议层 | ⏳ 代码已完成主要部分，硬件及端到端联调未验证 |
| Phase 4 | **SPI LCD + I2C 触摸驱动** | ⏳ 当前阶段 |
| Phase 5 | LVGL 移植 + 显示任务 | ⏳ 待开始 |
| Phase 6 | 仪表盘 UI 开发 + 集成测试 | ⏳ 待开始 |

### Phase 4 (当前)：SPI LCD + I2C 触摸驱动

- [ ] 将 `stm32f4xx_spi.c`、`stm32f4xx_i2c.c` 加入 Keil 工程；仅在选用 DMA 发送时再加入 `stm32f4xx_dma.c`
- [ ] 创建 `bsp_spi_lcd.c/h`：SPI2 初始化 + ILI9341V 初始化序列
- [ ] 创建 `bsp_i2c_touch.c/h`：I2C1 初始化 + FT6336G 坐标读取
- [ ] 添加 EXTI8/I2C ISR 空桩到 `firmware/cmsis/device/stm32f4xx_it.c`
- [ ] 编译验证（0 error 0 warning, 双 Target）

### Phase 5：LVGL 移植

- [ ] 复制 LVGL v8.x 源码到 `third_lib/lvgl/`
- [ ] 创建 `lv_conf.h`（RGB565, 240×320, 双缓冲 28.8KB）
- [ ] 创建 `lvgl_port_disp.c` + `lvgl_port_touch.c`
- [ ] 将 LVGL 源文件和移植文件加入 Keil 工程
- [ ] 编译验证

### Phase 6：仪表盘 + 集成

- [ ] 创建 `task_display.c`（5ms LVGL 任务）
- [ ] 修改 `task_entry.c`：新增初始化调用 + DISPLAY 任务
- [ ] 测试 UI：全屏纯色 → Label "Hello" → Arc 动画
- [ ] 触摸交互验证
- [ ] CAN 数据驱动 UI
- [ ] 30 分钟稳定性验证

详细实施计划见 `project/display_ecu_f429/docs/spi_touch_screen_plan.md`。

---

## 13. 烧录与下载方式

开发期首选 **SWD**（PA13/PA14），ISP 作为备用。

- **SWD**：插上 USB，Keil/IDE 选 ST-Link 直接烧，无需 BOOT 操作
- **ISP**：BOOT0=1 → 复位 → STM32CubeProgrammer 串口烧录 → BOOT0=0
- **OTA**：YMODEM 串口传输 + 内部 Flash 真 AB 分区，与 BOOT0 无关

---

## 14. 调试策略

### 14.1 常见问题

| 现象 | 排查 |
|------|------|
| F429 反复复位 | 检查 VCAP_1/VCAP_2 是否接 2.2 µF |
| SWD 连不上 | 检查 PA13/PA14/NRST；BOOT0 是否误设为 1 |
| **LCD 黑屏** | 检查 SPI2 时钟/数据、RST 复位时序、背光 BL 引脚 |
| **LCD 花屏** | 检查 SPI 模式 (CPHA=0,CPOL=0)、像素格式 (0x3A=0x55)、MADCTL (0x36=0x08) |
| **触摸无反应** | 检查 I2C1 SCL/SDA、FT6336G 芯片 ID (0xA8=0x11)、INT 配置 |
| CAN 收不到 | 检查 PA11/PA12 默认引脚配置、终端电阻、波特率 |
| CAN Bus-Off | 检查接线、终端电阻、CANH/CANL 反接 |
| Bootloader 跳转 HardFault | 检查 VTOR、栈指针、reset-handler 地址 |
| LVGL 断言失败 | `draw_buf` 不能放 CCM（DMA 不可达），必须主 SRAM |

### 14.2 LVGL 调试

- 开启 `LV_USE_PERF_MONITOR` 观察 FPS 和 CPU 占用
- 开启 `LV_LOG_LEVEL` 输出调试信息
- 关键约束：双缓冲 `draw_buf` 必须分配在主 SRAM (`0x2000xxxx`)，**禁止 CCM**

---

## 15. 采购清单

### 15.1 已购买

- STM32F429IGT6 核心板（板载 SDRAM/NAND，当前软件不使用）
- STM32F103C8T6 核心板
- TJA1050 CAN 收发器 ×2（采购/连接状态需现场确认）
- DRV8833 ×2 + MG310 ×2 电机套件（代码已实现，硬件未验证）
- ST-Link / USB-TTL

### 15.2 待购买

| 类别 | 型号 | 数量 | 说明 |
|------|------|------|------|
| SPI 触摸屏 | **MSP2834 (ILI9341V + FT6336G)** | 1 | 2.8 寸, SPI2 + I2C1；按用户手册建议 5V 供电 |
| 14P FPC 排线 | 0.5mm 间距, 反向 | 1 | 模块配件已包含 |

---

## 附录 A：关键参考文档

- STM32F429xx Reference Manual (RM0090)
- STM32F429IGT6 Datasheet
- MSP2834 用户手册 + 规格书：`docs/schematic/MSP2834/MSP2834/`
- ILI9341 数据手册：`docs/schematic/MSP2834/MSP2834/ILI9341_Datasheet.pdf`
- ILI9341V 初始化序列：`docs/schematic/MSP2834/MSP2834/ILI9341V_Init.txt`
- FT6336G 数据手册：`docs/schematic/MSP2834/MSP2834/D-FT6336G-DataSheet-V1.0.pdf`
- FT6336G 寄存器表：`docs/schematic/MSP2834/MSP2834/FT6336G_Register.xlsx`
- LVGL v8 官方文档：https://docs.lvgl.io/8.3/
- SPI 触摸屏实施计划：`project/display_ecu_f429/docs/spi_touch_screen_plan.md`
- 原 F407 方案（历史）：`docs/Car_Panel_Project_Plan.md`

---

> 文档版本历史：
> - v1.0：基于 F407 方案改写
> - v2.0–v2.4：LTDC RGB + SDRAM + W25Q64 方案（已废弃，方案变更）
> - **v3.0（历史）**：
>   - **方案变更**：LTDC/LED → MSP2834 SPI 2.8 寸触摸屏（ILI9341V + FT6336G）
>   - **移除**：SDRAM、NAND、DMA2D、W25Q64 全部不使用
>   - **OTA**：W25Q64 暂存 → 内部 Flash 真 AB 分区（已实现）
>   - **更新**：全部章节根据实际开发进度重写（Bootloader + CAN + FreeRTOS 已完成）
>   - **新增**：FT6336G 寄存器映射、ILI9341V 初始化序列、LVGL SPI 移植方案
>   - 新增 SPI 触摸屏实施计划：`project/display_ecu_f429/docs/spi_touch_screen_plan.md`
> - **v3.1（当前）**：
>   - 修正 F429 CAN 实际引脚为 PA11/PA12 默认映射
>   - 修正 VCAP_1/VCAP_2 为 Pin 81/125，并同步双电机动力域器件与当前 CAN 实现状态
