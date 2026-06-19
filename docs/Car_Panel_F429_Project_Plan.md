# Car_Panel 汽车双 ECU 仪表盘项目方案 —— STM32F429IGT6 显示域版本

> 项目代号：**Car_Panel-F429**  
> 文档版本：**v2.4**（修正 FMC/LTDC 引脚冲突、补充 CAN 协议和 OTA 详细机制）  
> 目标：在 **STM32F429IGT6（显示域）+ STM32F103C8T6（动力域）+ CAN 总线** 架构上，实现 OTA/IAP、LVGL 仪表盘 UI、LTDC RGB 屏、DMA2D 显示加速、闭环电机控制。  
> 本文档基于实际原理图（`extracted/schematic_for_ai.md`）重构，适配板载 SDRAM/NAND 资源。

---

## 目录

1. [选型说明：为什么用 F429IGT6](#1-选型说明为什么用-f429igt6)
2. [硬件资源清单与角色分配](#2-硬件资源清单与角色分配)
3. [F429IGT6 引脚规划（显示域）](#3-f429igt6-引脚规划显示域)
4. [电源树设计](#4-电源树设计)
5. [Flash 与外部存储分区](#5-flash-与外部存储分区)
6. [主方案系统架构](#6-主方案系统架构)
7. [LTDC + RGB LCD 配置](#7-ltdc--rgb-lcd-配置)
8. [DMA2D + LVGL + SDRAM 全屏双缓冲](#8-dma2d--lvgl--sdram-全屏双缓冲)
9. [W25Q64 Flash OTA + 升级标志方案](#9-w25q64-flash-ota--升级标志方案)
10. [CAN 通信协议草案](#10-can-通信协议草案)
11. [软件模块拆分](#11-软件模块拆分)
12. [开发阶段计划](#12-开发阶段计划)
13. [烧录与下载方式](#13-烧录与下载方式)
14. [调试策略](#14-调试策略)
15. [采购清单](#15-采购清单)

---

## 1. 选型说明：为什么用 F429IGT6

### 1.1 核心诉求

| 诉求 | F429IGT6 是否满足 |
|---|---|
| 不用 STM32F407 | ✅ 满足 |
| 驱动 RGB LCD 屏 | ✅ 通过 **LTDC** 原生支持 |
| 流畅 LVGL 运行 | ✅ 180 MHz + 256 KB RAM + **32MB SDRAM 全屏双缓冲** + **DMA2D** |
| 板上资源丰富 | ✅ SDRAM + NAND + 辅助 F103 + CH340 |
| 未来可扩展 | ✅ LTDC + DMA2D 可驱动更高分辨率屏 |

### 1.2 F429IGT6 vs F407ZGT6 vs F411RET6

| 项目 | F407ZGT6（原方案） | F411RET6（已验证 OTA） | **F429IGT6（本方案）** |
|---|---|---|---|
| 内核/主频 | Cortex-M4 @ 168 MHz | Cortex-M4 @ 100 MHz | **Cortex-M4 @ 180 MHz** |
| Flash | 1 MB | 512 KB | **1 MB** |
| RAM | 192 KB | 128 KB | **256 KB + 32 MB SDRAM** |
| 显示接口 | FSMC 8080 | 无 | **LTDC + FMC SRAM** |
| RGB/LTDC | 无 | 无 | **LTDC + DMA2D** |
| CAN | 2× bxCAN | 1× bxCAN | **2× bxCAN** |
| 板载存储 | SPI Flash | SPI Flash | **SDRAM + NAND + SPI模块** |
| 封装 | LQFP144 | LQFP64 | **LQFP176** |

### 1.3 选型结论

- **F429IGT6 是显示增强版**：内置 LTDC + DMA2D，配合 32MB SDRAM 可实现流畅 UI 动画。
- **板载资源丰富**：SDRAM 全屏双缓冲、NAND 大容量存储、辅助 F103 可分担任务。
- **唯一代价**：LQFP176 必须重新画板/飞线，价格比 F407/F412 高。

---

## 2. 硬件资源清单与角色分配

### 2.1 STM32F429IGT6 显示域（板载资源）

| 资源 | 型号 | 规格 | 备注 |
|------|------|------|------|
| 主频 | STM32F429IGT6 | 180 MHz | LQFP176 |
| Flash | 片上 | 1 MB | 0x08000000–0x080FFFFF |
| RAM | 片上 | 256 KB | 0x20000000–0x2003FFFF |
| **SDRAM** | **W9825G6KH-6** | **32 MB, 16-bit, 68 MHz** | **可做全屏双缓冲** |
| **NAND Flash** | **W29N01HVSINA** | **128 MB, 8-bit** | 后续 OTA 迁移目标 |
| 辅助 MCU | STM32F103CBT6 | LQFP48 | USB/UART 桥接（暂不使用）|
| USB UART | CH340N | - | PA9(TX)/PA10(RX) |
| LDO | AMS1117-3.3 | 3.3V | 板载稳压 |
| 5V 稳压 | MT9700 | 5V | 板载 |
| LTDC | 有 | - | 驱动 RGB LCD |
| DMA2D | 有 | - | 加速 LVGL flush |

**板上未焊接/未标注**：
- W25Q64 模块需外接（OTA 验证阶段使用）
- AT24C02 无（升级标志存储在 W25Q64 固定区块）

### 2.2 外置模块（需采购/使用现有）

| 模块 | 型号 | 接口 | 备注 |
|------|------|------|------|
| **RGB LCD** | 4.3/5/7 寸 RGB565 | LTDC | **本方案核心外设** |
| CAN 收发器 | TJA1050 / SN65HVD230 | GPIO + CAN1 | ×2 |
| BTS7960 / IBT-2 | H 桥驱动 | PWM + DIR | |
| JGA25-370 | 12V 电机 + AB 相编码器 | Encoder 接口 | |
| **W25Q64 模块** | SPI Flash 8 MB | SPI3 | **OTA 验证阶段使用** |
| F103C8T6 核心板 | 动力域 ECU | CAN + PWM | 现有 |

### 2.3 板上 LED 和按键

| 元件 | GPIO | 方向 | 备注 |
|------|------|------|------|
| LED2 (蓝) | PE2 | 输出 | **高电平点亮**（注意！）|
| LED3 | PH2 | 输出 | 低电平点亮 |
| LED7 | PF0 | 输出 | 低电平点亮 |
| LED8 | PC13/PA12 | - | 贴片 LED |
| KEY1 (WKUP) | PA0 | 输入 | **低电平有效**，4.7kΩ 上拉 |
| KEY2 | PI11 | 输入 | 1kΩ 上拉 |
| KEY3 | PA0_WP | 输入 | 同 WKUP |
| KEY4 | PC13 | 输入 | |
| RESET | NRST | 输入 | 复位按键 |

---

## 3. F429IGT6 引脚规划（显示域）

### 3.1 引脚分配总表

基于原理图（`extracted/schematic_for_ai.md`）整理：

| 模块 / 信号 | MCU 引脚 | 方向 | 备注 |
|-------------|---------|------|------|
| **电源 / 系统** | | | |
| 3V3 | VDD 多引脚 | - | 板载 LDO 输出 |
| GND | VSS 多引脚 | - | |
| VCAP_1 | Pin 31 | - | **必须接 2.2 µF 到地** |
| VCAP_2 | Pin 81 | - | **必须接 2.2 µF 到地** |
| VBAT | Pin 5 | - | 电池/3.3V |
| NRST | Pin 31 | 输入 | 复位 |
| BOOT0 | Pin 166 | - | 跳线帽默认 0 |
| BOOT1 | PB2 | - | |
| **SWD 调试** | | | |
| SWDIO | PA13 | 双向 | 调试下载 |
| SWCLK | PA14 | 输出 | 调试时钟 |
| **USART1 + CH340** | | | |
| USART1_TX | PA9 | 输出 | → CH340 RX |
| USART1_RX | PA10 | 输入 | ← CH340 TX |
| **CAN1 总线** | | | |
| CAN1_RX | PB8 | 输入 | Remap 模式 |
| CAN1_TX | PB9 | 输出 | Remap 模式 |
| **LTDC RGB LCD** | | | |
| LTDC_R0–R4 | PC0, PC2, **PI0**, PC6, PC7 | 输出 | RGB 数据（PC3 让给 FMC_SDCKE0）|
| LTDC_R5–R7 | PB0, PB1, PA6 | 输出 | |
| LTDC_G0–G5 | PA7, PB10, PB11, PG10, PG11, PG12 | 输出 | |
| LTDC_G6–G7 | PH2, PH3 | 输出 | |
| LTDC_B0–B4 | PB5, PB8*, PB9*, PB10*, PC10 | 输出 | 注意与CAN冲突 |
| LTDC_B5–B7 | PC11, PD3, PD6 | 输出 | |
| LTDC_CLK | PG7 | 输出 | Pixel Clock |
| LTDC_HSYNC | **PI3** | 输出 | Horizontal Sync（避开 PC6 冲突）|
| LTDC_VSYNC | PA4 | 输出 | Vertical Sync |
| LTDC_DE | PF2 | 输出 | Data Enable |
| LTDC_DISP | PF3 | 输出 | Display On |
| **SDRAM W9825G6KH（Bank1）** | | | |
| FMC_A0–A5 | PF0–PF5 | 输出 | 地址线 |
| FMC_A6–A11 | PF12, PF13, PF14, PF15, PG0, PG1 | 输出 | |
| FMC_A12 | PG2 | 输出 | |
| FMC_D0–D15 | PD14, PD15, PD0*, PD1*, PE7–PE15 | 双向 | 数据总线 |
| FMC_BA0/BA1 | PG4, PG5 | 输出 | Bank 地址 |
| FMC_NRAS / FMC_SDNRAS | PD5 | 输出 | **SDRAM RAS（行地址选通）**，NAND NWE（写使能）复用 |
| FMC_NCAS / FMC_SDNCAS | PD4 | 输出 | **SDRAM CAS（列地址选通）**，NAND NOE（读使能）复用 |
| FMC_NWE | PC6 | 输出 | NAND 独立写使能（避开 PD5） |
| FMC_NBL0/NBL1 | PE0, PE1 | 输出 | Byte Mask |
| FMC_SDCKE0 | PC3 | 输出 | SDRAM Clock Enable |
| FMC_SDCLK | PG8 | 输出 | SDRAM Clock |
| FMC_SDNCAS | PD4 | 输出 | SDRAM CAS（复用 NAND NOE）|
| FMC_SDNRAS | PD5 | 输出 | SDRAM RAS（复用 NAND NWE）|
| FMC_SDNWE | PC0 | 输出 | SDRAM 写掩码 |
| FMC_SDNE0 | **PB6** | 输出 | **SDRAM Bank1 片选（NE1）** |
| **NAND W29N01HV（Bank1，未使用）** | | | 后续 OTA 迁移目标 |
| FMC_NCE | PD6 | 输出 | NAND CE |
| FMC_ALE | PD12 | 输出 | Address Latch Enable |
| FMC_CLE | PD11 | 输出 | Command Latch Enable |
| FMC_NOE | PD4 | 输出 | **NAND 读使能（复用 SDRAM CAS）**，SDRAM 模式下由 FMC 自动控制 |
| FMC_NWE | PC6 | 输出 | **NAND 写使能（独立引脚）**，避开 PD5 与 SDRAM RAS 冲突 |
| FMC_DATA | PD14, PD15, PD0, PD1, PE7–PE15 | 双向 | 8-bit 数据 |
| **W25Q64 SPI Flash（OTA 验证）** | | | |
| SPI3_SCK | PC10 | 输出 | |
| SPI3_MISO | PC11 | 输入 | |
| SPI3_MOSI | PC12 | 输出 | |
| W25Q64_CS | **PI4** | 输出 | 任意 GPIO（原 PI3 改用于 LTDC_HSYNC）|
| **板载 LED** | | | |
| LED2 (蓝) | PE2 | 输出 | **高电平点亮** |
| LED3 | PH2 | 输出 | 低电平点亮 |
| LED7 | PF0 | 输出 | 低电平点亮 |
| **板载按键** | | | |
| KEY1 (WKUP) | PA0 | 输入 | 低电平有效 |
| KEY2 | PI11 | 输入 | |
| KEY4 | PC13 | 输入 | |
| **F103 辅助 MCU 通信** | — | — | 板载 F103CBT6 仅用于 USB/UART 桥接 CH340N，无独立引脚引出 |

> **注**：标 * 的引脚需根据实际 LCD RGB 配置调整

### 3.2 引脚冲突检查

| 冲突点 | 处理方案 |
|--------|---------|
| PA9/PA10 被 CH340 占用 | CAN1 必须用 PB8/PB9 Remap |
| PB8/PB9 同时用于 CAN1 和 RGB B通道 | 分配 B4/B5 给其他引脚，避开 PB8/PB9 |
| PD0/PD1 被片内 FMC 数据总线和 NAND 占用 | FMC 数据线从 PD14 开始，低 2 位 (PD0/PD1) 被 NAND 8-bit 总线占用，16-bit SDRAM 需全部 D0–D15，两者不能同时满带宽工作 |
| **PC3 被 LTDC_R2 和 FMC_SDCKE0 同时占用** | **已修复**：LTDC_R2 改到 PI0，PC3 专用于 FMC_SDCKE0 |
| LTDC_HSYNC(原 PC6) 与 LTDC_R4(原 PC6) 重复 | **已修复**：LTDC_HSYNC 改到 PI3 |
| **PB6 被 I2C0_SCL 和 FMC_SDNE0 同时占用** | ~~已修复~~ **已删除**：I2C0_SCL 不存在，板载 F103CBT6 无独立引脚引出，FMC_SDNE0 正常使用 PB6（NE1）|
| **PI3 被 W25Q64_CS 和 LTDC_HSYNC 同时占用** | **已修复**：W25Q64_CS 改到 PI4 |
| LTDC 与 FMC 共用部分 GPIO | 两者**必须共存**（framebuffer 在 SDRAM 里）。需仔细分配引脚，优先保证 SDRAM 控制信号，LTDC 数据线用备选引脚绕开 |
| PA5/PA6/PA7 被标注为 SPI1 | 板载 NAND 已占用，实际不可用 |
| PD4/PD5 复用 NAND NOE/NWE 与 SDRAM CAS/RAS | **已修复**：SDRAM CAS/RAS 复用 NAND NOE/NWE（由 FMC 自动控制），NAND 独立使用 PC6 作为 NWE |

### 3.3 RGB LCD 建议接线

| RGB LCD 信号 | 建议接法 | 备注 |
|-------------|---------|------|
| R0–R4 | PC0, PC2, PI0, PC6, PC7 | PC3 让给 FMC_SDCKE0 |
| R5–R7 | PB0, PB1, PA6 | |
| G0–G5 | PA7, PB10, PB11, PG10, PG11, PG12 | |
| G6–G7 | PH2, PH3 | |
| B0–B4 | PB5, PC10, PC11, PD3, PD6 | 避开 PB8/PB9（CAN）|
| B5–B7 | PI1, PI2, PI5 | |
| CLK | PG7 | |
| HSYNC | PI3 | 原 PC6 冲突 |
| VSYNC | PA4 | |
| DE | PF2 | |
| DISP | PF3 | |
| DE | PF2 |
| DISP | PF3 |
| GND | GND |
| 5V / 3.3V | 外部供电（需确认 LCD 电压）|

---

## 4. 电源树设计

### 4.1 板载电源架构

```
12V DC Input
    ↓
U11 (MT9700) → +5V
    ↓
U13 (AMS1117-3.3) → +3.3V → F429, SDRAM, NAND, CH340
    ↓
F429 VDDA → 模拟电源（磁珠隔离）
```

### 4.2 F429IGT6 供电要求

| 引脚 | 接法 | 说明 |
|------|------|------|
| VDD/VSS | 多对 | 每对就近放 100 nF 退耦 |
| VDDA/VSSA | 3.3V/地 | ADC 模拟电源，串磁珠 |
| VREF+ | 3.3V | 若不使用 ADC |
| **VCAP_1** | **2.2 µF → 地** | 内部稳压输出，必须接 |
| **VCAP_2** | **2.2 µF → 地** | 必须接 |
| VBAT | 3.3V 或纽扣电池 | 后备域 |

### 4.3 扩展模块供电

| 模块 | 电压 | 来源 |
|------|------|------|
| RGB LCD | 5V 或 3.3V | 外部电源（确认 LCD 规格）|
| W25Q64 模块 | 3.3V | F429 GPIO 或外部 LDO |
| CAN 收发器 | 3.3V 或 5V | 外部 LDO |

---

## 5. Flash 与外部存储分区

### 5.1 F429IGT6 片上 Flash 分区（1 MB）

```
0x08000000 - 0x0801FFFF   Bootloader    128 KB   (扇区 0–4)
0x08020000 - 0x080DFFFF   App            768 KB  (扇区 5–10)
0x080E0000 - 0x080FFFFF   参数/标志区    128 KB  (扇区 11)
```

### 5.2 SDRAM 分区（W9825G6KH 32 MB）

```
0xC0000000 - 0xC01FFFFF   LVGL Framebuffer 0   2 MB   (全屏双缓冲)
0xC0200000 - 0xC03FFFFF   LVGL Framebuffer 1   2 MB   (全屏双缓冲)
0xC0400000 - 0xC1FFFFFF   UI 资源/字库缓存   28 MB  (可选)
```

### 5.3 W25Q64 分区（OTA 验证阶段）

```
0x000000 - 0x0FFFFF   固件暂存区   1 MB
0x100000 - 0x1FFFFF   资源/字库    1 MB
0x200000 - 0x7FFFFF   预留         6 MB
```

### 5.4 NAND 分区（后续 OTA 迁移目标）

```
Block 0-1:     坏块表              256 KB
Block 2-10:    固件区 A             2 MB
Block 11-19:   固件区 B             2 MB
Block 20-127:  资源/字库           ~26 MB
Block 128:     升级标志区           128 KB (磨损均衡)
```

### 5.5 F103 辅助 MCU Flash（升级标志）

F103CBT6 片上 128 KB Flash：
```
0x08000000 - 0x0801FFFF   App + 标志存储
```

---

## 6. 主方案系统架构

```text
                    CAN 总线 (500 kbps)
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
| LVGL +    | <-------------------> | 编码器测速|
| DMA2D     |                       | PID 闭环  |
| LTDC RGB  |                       |           |
| CAN 接收  |                       |           |
| W25Q64    |                       |           |
| NAND      |                       |           |
+-----+-----+                       +-----+-----+
      │                                   │
      │ LTDC RGB                         │ PWM/DIR/Encoder
      ▼                                   ▼
  4.3/5/7 寸 RGB LCD              DC 减速电机 + H 桥
  (RGB565, 带触摸)                  (BTS7960 + JGA25-370)
```

---

## 7. LTDC + RGB LCD 配置

### 7.1 LTDC 参数计算（以 4.3 寸 480×272 为例）

| 参数 | 值 | 说明 |
|------|-----|------|
| 像素时钟 | 9 MHz | 需根据 LCD 规格调整 |
| HSW (Horizontal Sync) | 41 | LCD 规格 |
| HSP (Horizontal Back Porch) | 2 | LCD 规格 |
| HFP (Horizontal Front Porch) | 2 | LCD 规格 |
| VSW (Vertical Sync) | 10 | LCD 规格 |
| VSP (Vertical Back Porch) | 2 | LCD 规格 |
| VFP (Vertical Front Porch) | 2 | LCD 规格 |
| 水平有效像素 | 480 | LCD 规格 |
| 垂直有效像素 | 272 | LCD 规格 |

### 7.2 LTDC 配置代码框架

```c
void MX_LTDC_Init(void)
{
    LTDC_HandleTypeDef hltdc;

    // Pixel clock: HSE(25MHz) / 2 / 2 = 6.25 MHz (或 PLL 配置)
    // 需根据实际 LCD 调整

    // Layer 1 配置 (RGB565)
    hltdc.Instance = LTDC;
    hltdc.Init.HorizontalSync = 41 - 1;
    hltdc.Init.VerticalSync = 10 - 1;
    hltdc.Init.AccumulatedHBP = 43;
    hltdc.Init.AccumulatedVBP = 12;
    hltdc.Init.AccumulatedActiveW = 523;
    hltdc.Init.AccumulatedActiveH = 284;
    hltdc.Init.TotalWidth = 525;
    hltdc.Init.TotalHeigh = 286;

    hltdc.LayerCfg[0].ImageWidth  = 480;
    hltdc.LayerCfg[0].ImageHeight = 272;

    HAL_LTDC_Init(&hltdc);

    // 设置 Framebuffer 地址 (SDRAM)
    HAL_LTDC_SetAddress(&hltdc, SDRAM_FRAMEBUFFER_ADDR);
}
```

### 7.3 CubeMX 配置要点

1. **RCC**：HSE = 25 MHz（板载晶振），配置 PLL 输出 180 MHz
2. **LTDC**：开启 LTDC 外设
3. **DMA2D**：开启 DMA2D 用于加速 flush
4. **FMC**：SDRAM Bank1 配置（W9825G6KH 参数）
5. **GPIO**：配置 LTDC 引脚映射

---

## 8. DMA2D + LVGL + SDRAM 全屏双缓冲

### 8.1 核心优势

有了 32 MB SDRAM，LVGL 可以配置**全屏双缓冲**。

> ⚠️ **内存位置注意**：两个全屏缓冲 `draw_buf[2][480×272]` 占 **~522 KB**（480×272×2 字节×2），远超 F429 片上 RAM（256 KB）。**draw_buf 必须定义在 SDRAM 地址空间**（使用 `__attribute__((section(".sdram")))` 或链接脚本指定），否则链接失败或运行中 Stack Overflow。

```c
// lv_conf.h
#define LV_COLOR_DEPTH            16
#define LV_DISP_DEF_REFR_PERIOD  5    // 5 ms 刷新

// 全屏双缓冲 (480 × 272 × 2 bytes = ~260 KB × 2 = ~522 KB → 必须放 SDRAM)
// 方式一：链接脚本指定 .lvgl_buf 段到 SDRAM
// 方式二：动态分配在 SDRAM 堆上
#define LVGL_BUF_LINES    272
static lv_color_t draw_buf[2][LV_HOR_RES_MAX * LVGL_BUF_LINES]
    __attribute__((section(".lvgl_buf")));   // 链接脚本映射到 SDRAM (>=0xC0400000)
static lv_disp_draw_buf_t disp_buf;

// 初始化时：
lv_disp_draw_buf_init(&disp_buf, draw_buf[0], draw_buf[1],
                       LV_HOR_RES_MAX * LVGL_BUF_LINES);
```

### 8.2 性能目标

| 指标 | 目标值 |
|------|--------|
| LVGL FPS | ≥ 50 fps |
| CPU 占用 | < 60% |
| DMA2D 占用 | < 30% |

### 8.3 DMA2D flush_cb 实现

```c
#include "lvgl.h"
#include "stm32f4xx_hal.h"

#define SDRAM_FRAMEBUFFER_ADDR   0xC0000000

static DMA2D_HandleTypeDef hdma2d;

void lvgl_dma2d_init(void)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();

    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode         = DMA2D_M2M;                 // Memory to Memory
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = LV_HOR_RES_MAX - 480;      // 初始值：全屏刷新 offset=0，
                                                           // 局部刷新时在 flush_cb 中动态修改
    HAL_DMA2D_Init(&hdma2d);
}

void lcd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width  = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t dst = SDRAM_FRAMEBUFFER_ADDR
                 + ((area->y1 * LV_HOR_RES_MAX + area->x1) * 2);

    /* 关键：输出行偏移 = 屏宽 - 本次刷新宽度。
       全屏刷新时 OOR=0；局部刷新时不设会导致行错位/花屏 */
    hdma2d.Instance->OOR = LV_HOR_RES_MAX - width;

    // DMA2D 中断搬运到 SDRAM Framebuffer — CPU 立即返回，搬运完成后在回调中通知 LVGL
    HAL_DMA2D_Start_IT(&hdma2d,
                       (uint32_t)color_p,
                       dst,
                       width,
                       height);
    // 注意：不要在这里调用 lv_disp_flush_ready()！等 HAL_DMA2D_TransferCompleteCallback 回调
}

/* DMA2D 搬运完成中断回调 — 在这里通知 LVGL 本帧完成 */
void HAL_DMA2D_TransferCompleteCallback(DMA2D_HandleTypeDef *hdma2d)
{
    lv_disp_flush_ready(&disp_drv);
}
```

> **为什么用中断而非轮询**：`HAL_DMA2D_PollForTransfer()` 会阻塞 CPU 等待搬运完成，等于 DMA2D 搬运期间 CPU 空转，浪费了 DMA2D 释放 CPU 的初衷。用 `_IT` 中断方式 → CPU 投递任务后立即返回，可同时处理 CAN 收发、按键扫描等任务。

### 8.4 SDRAM 初始化

```c
void FMC_SDRAM_Init(void)
{
    FMC_SDRAM_CommandTypeDef cmd;
    FMC_SDRAM_TimingTypeDef timing;

    // W9825G6KH: 32MB, 16-bit, CL=3
    timing.LoadToActiveDelay    = 2;
    timing.ExitSelfRefreshDelay  = 7;
    timing.SelfRefreshTime       = 4;
    timing.RowCycleDelay         = 7;
    timing.WriteRecoveryTime     = 3;
    timing.RPDelay               = 2;
    timing.RCDDelay              = 2;

    // 时钟配置
    cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100);
    HAL_Delay(1);

    // PALL
    cmd.CommandMode = FMC_SDRAM_CMD_PALL;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100);

    // Auto Refresh
    cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    cmd.AutoRefreshNumber = 8;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100);

    // Load Mode Register
    // W9825G6KH: Burst length=1, CAS=3
    cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    HAL_SDRAM_SendCommand(&hsdram1, &cmd, 100);

    // Refresh rate
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, 1386);  // 64ms / 4096 rows
}
```

---

## 9. W25Q64 Flash OTA + 升级标志方案

### 9.1 硬件连接

| W25Q64 模块 | F429 引脚 |
|-------------|----------|
| VCC | 3.3V |
| GND | GND |
| SI (MOSI) | PC12 |
| SO (MISO) | PC11 |
| CLK | PC10 |
| CS | PI4 |

### 9.2 分区设计

```
W25Q64 (8 MB):
┌─────────────────────┬─────────────────────┐
│  0x000000 - 0x00FFF │  升级标志区          │   4 KB
├─────────────────────┼─────────────────────┤
│  0x010000 - 0x07FFF │  OTA 固件区 A       │ 448 KB
├─────────────────────┼─────────────────────┤
│  0x080000 - 0x0FFFF │  OTA 固件区 B       │ 512 KB
├─────────────────────┼─────────────────────┤
│  0x100000 - 0x1FFFFF │  字库/资源区        │  1 MB
├─────────────────────┼─────────────────────┤
│  0x200000 - 0x7FFFFF │  预留               │  6 MB
└─────────────────────┴─────────────────────┘
```

### 9.3 升级标志结构

```c
// W25Q64 固定区块存储结构
typedef struct {
    uint8_t  magic[4];        // 'OTA_' 标记
    uint8_t  target_slot;      // 目标槽位 (0=A, 1=B)
    uint32_t fw_size;          // 固件大小
    uint32_t fw_crc32;        // CRC32 校验
    uint32_t fw_addr;         // 固件在 W25Q64 中的地址
    uint8_t  status;           // 0=idle, 1=receiving, 2=ready, 3=updating
    uint8_t  write_count;      // 写入计数（磨损均衡）
    uint8_t  reserved[6];       // 对齐到 32 字节
} ota_flag_t;

#define OTA_FLAG_ADDR    0x000000    // W25Q64 起始 4KB 区块
```

### 9.4 OTA 完整流程（含双槽位回退）

```
┌─────────────────────────────────────────────────────────────────┐
│                        App 侧升级流程                           │
├─────────────────────────────────────────────────────────────────┤
│ 1. App 检测到升级命令（CAN / UART / 按键）                       │
│ 2. 读取当前运行槽位（从 Flash 参数区）                          │
│ 3. 选择目标槽位：若运行在 A，则升级 B；反之亦然                   │
│ 4. 擦除目标槽位 W25Q64 区域（448 KB）                          │
│ 5. YMODEM 接收固件 → 逐块写入 W25Q64 目标槽位                   │
│ 6. 固件接收完成 → 计算 CRC32 并与传输的校验和比对                │
│ 7. CRC 校验通过 → 写入升级标志（target_slot, status=ready）      │
│ 8. CRC 校验失败 → 擦除目标槽位，记录错误日志，保持运行           │
│ 9. NVIC_SystemReset()                                          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                      Bootloader 侧流程                          │
├─────────────────────────────────────────────────────────────────┤
│ 1. Bootloader 启动，读取 W25Q64 升级标志                        │
│ 2. 检查 magic 标记 ('OTA_') 是否存在                            │
│ 3. 若无标记或 status≠ready → 直接跳转 App（正常启动）           │
│ 4. 若有标记：                                                   │
│    a. 校验 CRC32（W25Q64 固件 vs 存储的 crc32）                │
│    b. CRC 校验失败 → 清除标志，记录错误 → 跳转 App              │
│    c. CRC 校验通过 → 进入升级模式                                │
│ 5. 擦除 App Flash 区（768 KB）                                 │
│ 6. 从 W25Q64 逐块复制固件到 App Flash（DMA 加速）              │
│ 7. 复制完成后，再次校验 App Flash CRC32                         │
│ 8. CRC 校验通过 → 清除升级标志 → 更新运行槽位标记 → 跳转 App    │
│ 9. CRC 校验失败 → 标记升级失败 → 可选择回退到旧固件            │
└─────────────────────────────────────────────────────────────────┘
```

### 9.5 A/B 槽选择逻辑

```c
typedef enum {
    SLOT_A = 0,
    SLOT_B = 1,
} ota_slot_t;

ota_slot_t ota_get_current_slot(void) {
    // 从 Flash 参数区读取当前运行槽位
    return *(volatile uint8_t*)(APP_PARAM_ADDR);
}

ota_slot_t ota_get_target_slot(void) {
    // 目标槽位 = 当前槽位的反
    return (ota_get_current_slot() == SLOT_A) ? SLOT_B : SLOT_A;
}

// W25Q64 槽位地址
#define SLOT_ADDR(slot)   ((slot) == SLOT_A ? OTA_SLOT_A_ADDR : OTA_SLOT_B_ADDR)
```

### 9.6 搬运中断处理（掉电保护）

为防止搬运过程中掉电导致固件损坏，Bootloader 采用**两阶段校验**：

| 阶段 | 校验点 | 失败处理 |
|------|--------|---------|
| W25Q64 → RAM | CRC32(W25Q64) vs ota_flag.fw_crc32 | 中止升级，保持原 App |
| RAM → App Flash | CRC32(App Flash) vs ota_flag.fw_crc32 | 中止升级，擦除 App Flash |
| App 首次启动 | App 内置自检 | 回退到旧槽位（若双槽均有效）|

> **掉电恢复机制**：若搬运过程中意外掉电，下次 Bootloader 启动时会发现 App Flash CRC32 校验失败。此时若原槽位（当前未运行）有效，自动回退到原固件。

### 9.7 磨损均衡策略

W25Q64 擦写次数约 10 万次，但仍需优化：
- **双槽位**：A/B 两个固件区，失败可回退
- **单标志区**：固定使用起始 4KB 区块（4KB 仅占 1 个扇区）
- **写入计数**：记录 `write_count`，用于评估升级频率
- **备用标志区**：预留 0x1000–0x1FFF 作为轮换标志区，降低单点磨损

### 9.8 W25Q64 驱动关键函数

```c
// bsp_w25q64.h
#define W25Q64_CS_PORT    GPIOI
#define W25Q64_CS_PIN     GPIO_PIN_4

#define OTA_FLAG_ADDR      0x000000
#define OTA_SLOT_A_ADDR   0x010000
#define OTA_SLOT_B_ADDR   0x080000

void bsp_w25q64_init(void);
void bsp_w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len);
void bsp_w25q64_write(uint32_t addr, uint8_t *buf, uint32_t len);
void bsp_w25q64_erase_sector(uint32_t addr);
void bsp_w25q64_erase_chip(void);
uint32_t bsp_w25q64_read_id(void);

// OTA 标志操作
ota_flag_t* ota_flag_read(void);
int ota_flag_write(ota_flag_t *flag);
void ota_flag_clear(void);

// OTA 搬运
int ota_copy_to_flash(ota_slot_t target_slot, void (*progress_cb)(uint8_t percent));
```

### 9.9 后续迁移到 NAND（时间表）

| 阶段 | 内容 | 目标时间 |
|------|------|---------|
| Phase 1 | W25Q64 OTA 验证完成，稳定运行 1 个月 | 第 1–2 个月 |
| Phase 2 | NAND 驱动适配，坏块表设计 | 第 3 个月 |
| Phase 3 | NAND OTA 双槽位实现 | 第 4 个月 |
| Phase 4 | W25Q64 降级为字库/资源存储，NAND 正式上线 | 第 5 个月 |

---

## 10. CAN 通信协议草案

### 10.1 基础参数

| 参数 | 值 |
|------|-----|
| 波特率 | 500 kbps |
| 显示域节点 | 0x0A |
| 动力域节点 | 0x0B |
| 终端电阻 | 120 Ω × 2 (总线两端) |

### 10.2 CAN 帧 ID 定义

| 帧类型 | CAN ID | 方向 | DLC | 说明 |
|--------|--------|------|-----|------|
| 电机控制帧 | **0x100** | F429 → F103 | 8 | 显示域发送控制命令 |
| 电机状态帧 | **0x180** | F103 → F429 | 8 | 动力域上报电机状态 |
| 诊断请求帧 | 0x181 | F429 → F103 | 8 | 诊断命令 |
| 诊断响应帧 | 0x181 | F103 → F429 | 8 | 诊断数据 |
| 心跳帧 | 0x1FF | F103 → F429 | 2 | F103 存活确认 |

### 10.3 帧字段详细定义

#### 电机控制帧 (ID: 0x100, 8 字节)

| 字节 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | enable | uint8_t | 电机使能：0=停止，1=使能 |
| 1 | direction | uint8_t | 方向：0=正向，1=反向 |
| 2–3 | target_speed | int16_t | 目标速度 (rpm) |
| 4–5 | reserved | int16_t | 预留（PWM 占空比扩展）|
| 6–7 | crc16 | uint16_t | CRC16 校验 (Modbus) |

#### 电机状态帧 (ID: 0x180, 8 字节)

| 字节 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | status | uint8_t | 运行状态：0=停止，1=运行，2=错误 |
| 1 | error_code | uint8_t | 错误码：0=正常，1=过流，2=堵转 |
| 2–3 | current_speed | int16_t | 实际速度 (rpm) |
| 4–5 | current | int16_t | 电流 (mA) |
| 6–7 | crc16 | uint16_t | CRC16 校验 |

#### 心跳帧 (ID: 0x1FF, 2 字节)

| 字节 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | heartbeat | uint8_t | 计数器，每 100ms 递增 |
| 1 | power_voltage | uint8_t | 电源电压 × 10 (如 120 = 12.0V) |

### 10.4 CAN 过滤器配置（F429）

```c
CAN_FilterTypeDef filter = {0};
filter.FilterBank = 0;
filter.FilterMode = CAN_FILTERMODE_IDLIST;
filter.FilterScale = CAN_FILTERSCALE_32BIT;
// 32 位模式下，IdHigh 和 IdLow 各存一个 ID
filter.FilterIdHigh = (0x180 << 5);         // 电机状态帧
filter.FilterIdLow  = (0x1FF << 5);         // 心跳帧
filter.FilterMaskIdHigh = 0xFFFF;
filter.FilterMaskIdLow = 0xFFFF;
filter.FilterFIFOAssignment = CAN_RX_FIFO0;
filter.FilterActivation = ENABLE;
filter.SlaveStartFilterBank = 14;
HAL_CAN_ConfigFilter(&hcan1, &filter);
```

> **注意**：
> - CAN1 必须使用 PB8/PB9（Remap 模式），因为 PA9/PA10 被 CH340 占用
> - 诊断帧 (0x181) 可在应用层根据需求灵活处理
> - 发送帧 (0x100) 无需配置过滤器，F103 会自动接收

---

## 11. 软件模块拆分

### 11.1 仓库目录结构

```
Car_Panel/
├── docs/
│   ├── Car_Panel_Project_Plan.md      # 原 F407 方案
│   ├── Car_Panel_F429_Project_Plan.md # 本文档 (v2.1)
│   └── schematic/
├── display_ecu_f429/                  # 显示域工程
│   ├── bootloader/
│   ├── app/
│   ├── common/
│   └── tools/
├── power_ecu_f103/                    # 动力域工程（不变）
└── tools/
```

### 11.2 显示域 F429 App 工程结构

```
display_ecu_f429/
├── app/
│   ├── Core/Src/
│   │   ├── main.c
│   │   ├── freertos.c
│   │   ├── bsp_can.c
│   │   ├── bsp_ltdc.c          # LTDC 配置
│   │   ├── bsp_sdram.c         # SDRAM 初始化
│   │   ├── bsp_dma2d.c         # DMA2D 封装
│   │   ├── bsp_touch.c
│   │   ├── bsp_w25q64.c        # OTA + 升级标志
│   │   ├── bsp_key.c
│   │   ├── bsp_led.c
│   │   ├── bsp_uart.c
│   │   ├── bsp_log.c
│   │   ├── can_app.c
│   │   ├── lvgl_port.c
│   │   └── ui/
│   │       ├── ui_main.c
│   │       ├── ui_diag.c
│   │       ├── ui_ota.c
│   │       └── ui_assets.c
│   ├── lvgl/
│   └── MDK-ARM/
└── common/
    ├── protocol/
    ├── bsp/
    └── linker/
```

### 11.3 FreeRTOS 任务规划

| 任务 | 优先级 | 栈 (words) | 周期 | 说明 |
|------|-------|-----------|------|------|
| CAN_Rx | 5 | 512 | 事件驱动 | |
| LVGL_Tick | 3 | 256 | 5 ms | |
| LVGL_Task | 2 | 4096 | 事件驱动 | 全屏刷新，栈增大 |
| UI_Update | 3 | 1024 | 16 ms | 60 fps 目标 |
| Key_Scan | 4 | 256 | 20 ms | |
| Touch_Scan | 4 | 512 | 10 ms | |
| OTA_Task | 3 | 1024 | 事件驱动 | |
| Log_Task | 1 | 512 | 100 ms | |
| Watchdog | 6 | 128 | 1000 ms | |
| Idle | 0 | 256 | — | |

FreeRTOS 堆建议 **60–80 KB**（片上 RAM + SDRAM）。

---

## 12. 开发阶段计划

> **里程碑时间表**（可根据实际情况调整）

| 阶段 | 内容 | 预计周期 |
|------|------|---------|
| Phase 0 | 硬件验证 | 1 周 |
| Phase 1 | LTDC + RGB LCD | 1 周 |
| Phase 2 | DMA2D + LVGL 最小显示 | 1–2 周 |
| Phase 3 | Bootloader + 最小 App | 1 周 |
| Phase 4 | W25Q64 OTA 验证 | 2 周 |
| Phase 5 | 动力域 + CAN 闭环 | 2 周 |
| Phase 6 | 完整 UI + 集成 | 2–3 周 |

### Phase 0：硬件验证

- [ ] F429 最小系统上电，确认 VCAP 已接
- [ ] 串口打印 Hello（CH340）
- [ ] SWD 下载调试验证
- [ ] SDRAM 读写测试
- [ ] 点亮板上 LED（注意高电平点亮）

### Phase 1：LTDC + RGB LCD

- [ ] LTDC 时钟配置
- [ ] GPIO LTDC 引脚映射（参考 §3.3 接线表）
- [ ] 配置 4.3 寸 RGB LCD（480×272）
- [ ] 显示纯色测试

### Phase 2：DMA2D + LVGL 最小显示

- [ ] 移植 LVGL v8
- [ ] 配置 SDRAM 全屏双缓冲
- [ ] 实现 DMA2D flush_cb
- [ ] 显示主仪表页，测量 FPS（目标 ≥ 50）

### Phase 3：Bootloader + 最小 App

- [ ] 编写 F429 Bootloader（128 KB）
- [ ] 编写最小 App（从 0x08020000 启动）
- [ ] 验证 VTOR 设置、跳转

### Phase 4：W25Q64 OTA 验证

- [ ] W25Q64 SPI Flash 驱动（CS = PI4）
- [ ] YMODEM 协议实现
- [ ] OTA 完整流程验证（固件接收、CRC32 校验）
- [ ] 升级标志读写验证
- [ ] Bootloader 固件搬运验证
- [ ] 双槽位回退机制验证

### Phase 5：动力域 + CAN 闭环

- [ ] F103 CAN 通信
- [ ] 电机 PWM 控制
- [ ] 编码器测速
- [ ] PID 闭环

### Phase 6：完整 UI + 集成

- [ ] 仪表盘页面开发
- [ ] 触摸驱动
- [ ] 完整 UI 动画
- [ ] 性能优化

---

---

## 13. 烧录与下载方式

F429 有两种烧录途径，**开发期首选 SWD（一键）**，ISP 作为无调试器时的备用手段。

### 13.1 两种方式对比

| 对比项 | **SWD（推荐）** | ISP / 系统 bootloader |
|--------|----------------|----------------------|
| 接口 | SWDIO(PA13) + SWCLK(PA14) + NRST | USART1(PA9/PA10, 经 CH340) 或 USB |
| 是否一键 | ✅ 插上 USB，IDE 选 SWD 直接烧 | ❌ 通常需配合 BOOT 引脚 |
| 是否需要动 BOOT0 | 不需要 | **需要**（BOOT0=1 进入 bootloader）|
| 烧录速度 | 快 | 慢（串口波特率限制）|
| 能否在线调试 | ✅ 支持断点/单步 | ❌ 不能 |
| 适用场景 | 日常开发、调试 | 无 SWD 调试器时的应急烧录 |

### 13.2 方式一：SWD 一键烧录（首选）

**前提**：板载调试器（板载 F103 桥接或外接 ST-Link/J-Link/DAP）已通过 SWD 接到 F429 的 PA13/PA14/NRST。

**操作**：
1. 一根 USB 线连到调试器（板载 USB 口）
2. Keil/CubeIDE/STM32CubeProgrammer 里 Debug 选 **ST-Link**（或 CMSIS-DAP）
3. 点 Download，**无需碰 BOOT 跳线**，直接烧进 Flash
4. 烧完自动复位运行

> **BOOT 引脚状态**：SWD 烧录时 BOOT0 保持 0（正常从 Flash 启动）即可。SWD 通过 NRST + SWDIO/SWCLK 强制接管内核读写 Flash，与 BOOT0 无关。

### 13.3 方式二：ISP 串口烧录（备用）

**前提**：仅有一条 USB-TTL（CH340 接 PA9/PA10），没有 SWD 调试器。

**F429 进入系统 bootloader 的条件**：

| BOOT0 | BOOT1(=PB2) | 启动方式 |
|-------|-------------|---------|
| 0 | X | **正常从 Flash 启动**（运行用户程序）|
| **1** | **0** | **从系统存储器启动（ISP bootloader）** |
| 1 | 1 | 从内置 SRAM 启动 |

**操作**：
1. 跳线/按键把 **BOOT0 拉到 1**，BOOT1(PB2) 保持 0
2. 按复位（或重新上电），F429 进入 ROM bootloader（地址 0x1FFF0000）
3. 打开 STM32CubeProgrammer / Flash Loader Demo，选 USART，连 PA9/PA10
4. 烧录 .hex/.bin
5. **BOOT0 拉回 0**，复位运行

> ⚠️ ISP 方式**不能一键**：每次烧录前后都要切换 BOOT0，否则复位后跑的是旧程序或停在 bootloader。

### 13.4 BOOT 跳线说明（本板）

| 信号 | 位置 | 默认 | 说明 |
|------|------|------|------|
| BOOT0 | 独立引脚（Pin 166），板载跳线/按键 | **0** | 0=Flash 运行，1=进入 ISP |
| BOOT1 | PB2 | 0 | ISP 模式下需为 0 |

**建议**：板上 BOOT0 默认接 0（跳线帽常态）。只有走 ISP 串口烧录时才临时切到 1，烧完务必切回，否则下次复位会停在 bootloader，App 不启动。

### 13.5 与 OTA 的关系

| 烧录方式 | 用途 |
|---------|------|
| SWD | 开发期烧 Bootloader、烧 App、在线调试 |
| ISP | 无 SWD 时的应急烧录（基本不用）|
| **OTA（W25Q64 + YMODEM）** | **产品期远程升级，完全不依赖 SWD/ISP/BOOT** |

> OTA 走的是 App → W25Q64 → Bootloader 搬运的软件通道，**与 BOOT0 无关**，是最终交付的升级方式。

---

## 14. 调试策略

### 14.1 常见问题

| 现象 | 排查 |
|---|---|
| F429 反复复位 | 检查 VCAP_1/VCAP_2 是否接 2.2 µF |
| SWD 连不上 | 检查 PA13/PA14/NRST 接线；BOOT0 是否误设为 1 |
| ISP 烧录识别不到 | 确认 BOOT0=1 且已复位进入 bootloader，串口波特率 |
| 复位后停在 bootloader/不启动 App | BOOT0 没切回 0 |
| 黑屏 | 检查 LCD 电源、LTDC 时钟、引脚映射 |
| 花屏 | 检查 RGB 时序参数（HSYNC/VSYNC/DE）|
| LVGL FPS 低 | 检查 DMA2D 是否启用，SDRAM 是否正常 |
| CAN 收不到 | 检查 PB8/PB9 Remap 配置、终端电阻 |
| CAN Bus-Off | 检查 CAN 总线接线、终端电阻、CANH/CANL 反接；重启 CAN 外设 |
| Bootloader 跳转 App HardFault | 检查 VTOR 是否设置、栈指针是否合法 |
| SDRAM 读写错误 | 检查 FMC SDRAM 初始化时序 |
| OTA 升级后白屏 | 检查 App Flash CRC32 是否正确，新固件是否兼容 |
| OTA 升级后卡在 Bootloader | 检查 W25Q64 固件区是否损坏，尝试重新升级 |

### 14.2 LVGL 调试

- 开启 `LV_USE_PERF_MONITOR` 观察 FPS 和 CPU 占用
- 开启 `LV_USE_ASSERT_HANDLER` 捕获断言失败
- 常见 LVGL 断言失败原因：
  - `draw_buf` 放错内存区域（需在 SDRAM）
  - `lv_area_t` 坐标超出屏幕范围
  - 多线程访问同一对象未加锁

### 14.3 CAN Bus-Off 处理

CAN 控制器进入 Bus-Off 状态后需自动恢复：

```c
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t err = HAL_CAN_GetError(hcan);

    if (err & HAL_CAN_ERROR_BOF) {
        // Bus-Off：关闭 → 重新初始化 → 启动
        HAL_CAN_Stop(&hcan1);
        HAL_CAN_Start(&hcan1);
        LOG_W("CAN Bus-Off, reconnected");
    }
}
```

### 14.4 OTA 调试

| 问题 | 排查 |
|------|------|
| YMODEM 传输失败 | 检查串口流控、缓冲区大小、超时设置 |
| CRC32 校验失败 | 传输过程干扰，检查线缆质量 |
| Bootloader 跳转后 HardFault | 检查 App 入口函数 `Reset_Handler` 地址、栈指针 |
| 升级后 App 异常 | 检查 `SystemInit()` 中 SCB->VTOR 设置 |

### 14.5 性能测量

- 开启 `LV_USE_PERF_MONITOR` 观察 FPS 和 CPU 占用
- 目标：RGB LCD 全屏 ≥ 50 fps，CPU 占用 < 60%

---

## 15. 采购清单

### 15.1 必买清单

| 类别 | 型号 | 数量 | 说明 |
|------|------|------|------|
| RGB LCD | 4.3/5/7 寸 RGB565 带触摸 | 1 | 本方案核心 |
| W25Q64 模块 | SPI Flash 8 MB | 1 | OTA 验证阶段 |
| CAN 收发器 | TJA1050 / SN65HVD230 | 2 | 与原方案一致 |

### 15.2 已有清单（不需再买）

- STM32F429IGT6 核心板（带 SDRAM/NAND）
- STM32F103C8T6 核心板
- TJA1050 ×1
- BTS7960 / JGA25-370
- ST-Link / USB-TTL

---

## 附录 A：关键参考文档

- STM32F429xx Reference Manual (RM0090)
- STM32F429IGT6 Datasheet
- STM32F4xx HAL Library User Manual
- W9825G6KH SDRAM Datasheet
- W29N01HV NAND Flash Datasheet
- LVGL v8 官方文档：https://docs.lvgl.io/8.3/
- 原方案：`Car_Panel_Project_Plan.md`

## 附录 B：原理图来源

- `C:\Users\LD1702\Desktop\stm32f429\extracted\schematic_for_ai.md`
- 高清原理图：`C:\Users\LD1702\Desktop\stm32f429\extracted\pages_png\page_01.png`

---

> 文档版本历史：
> - v1.0：基于 F407 方案改写
> - v2.0：基于实际原理图重构，适配 LTDC RGB 屏 + SDRAM 全屏双缓冲
> - v2.1：简化方案，升级标志存储改用 W25Q64（暂不使用 F103 I2C）
> - v2.2：新增第 13 章「烧录与下载方式」，明确 SWD 一键烧录 vs ISP（需 BOOT0 配合）
> - v2.3：修正 §8.3 DMA2D 轮询→中断+OOR、§8.1 draw_buf 必须放 SDRAM、§3.1/§5.2 SDRAM Bank2→Bank1 及 FMC_SDNWE(PC3→PC0)、§3.2 PD0/PD1 HSE 说法错误、§3.2 PC3 冲突重写、§3.1 FMC_SDCLK 重复行删除
> - **v2.4（本次）**：
>   - 修正 FMC 引脚冲突：FMC_SDNE0(B6)；NAND NWE 独立到 PC6
>   - 修正 LTDC 引脚：HSYNC 改到 PI3，LTDC_R2 改到 PI0，避开 PC3 冲突
>   - W25Q64_CS 从 PI3 改为 PI4
>   - **删除错误的 F103CBT6 I2C 连接**：板载 F103CBT6 仅用于 USB/UART 桥接 CH340N，无独立引脚引出
>   - 新增 CAN 协议完整帧定义（ID 0x100/0x180/0x1FF 字段表）
>   - 新增 OTA 双槽位回退机制、A/B 槽选择逻辑、搬运中断处理（掉电保护）
>   - 新增 OTA 调试章节、NAND 迁移时间表
>   - 新增 CAN Bus-Off 处理、LVGL 断言调试
>   - 新增开发阶段里程碑时间表
