# SPI 2.8 寸电容触摸屏过渡方案

> 基于 MSP2834 模块实际数据手册（ILI9341V + FT6336G），原 LTDC 方案废弃。

---

## 硬件确认（依据数据手册）

### MSP2834 模块规格

| 参数 | 值 | 来源 |
|------|-----|------|
| LCD 控制器 | **ILI9341V** | `MSP2833_MSP2834_规格书_CN_V1.0.pdf` |
| 分辨率 | **240×320** | 同上 |
| 显示颜色 | 65K（RGB565，16bit） | 同上 |
| 接口 | 4 线 SPI | 同上 |
| 有效显示区 | 43.20(W) × 57.60(H) mm | 同上 |
| 像素尺寸 | 0.153(H) × 0.153(V) mm | 同上 |
| 可视角度 | ALL O'CLOCK（IPS） | 同上 |
| 背光亮度 | 280 cd/m² | 同上 |
| 背光类型 | White LED×4，80mA | 同上 |
| 触摸控制器 | **FT6336G** | 同上 |
| 触摸接口 | I2C（最高 400kHz） | 同上 |
| 触摸分辨率 | 240×320 | 同上 |
| 模块工作电压 | **5V**（板载 3.3V LDO） | 同上 |
| 外形尺寸 | 50.0 × 86.0 × 14.28 mm | 同上 |
| SKU | MSP2834（有触摸）/ MSP2833（无触摸） | 同上 |

### SPI 通信协议（ILI9341 用户手册确认）

- **SPI 模式**：CPHA=0, CPOL=0（即 SPI 模式 0），MSB 优先
- **4 线制**：CS（片选, 低有效）+ DC/RS（命令/数据选择）+ SCK + MOSI
- DC/RS 低电平=命令，高电平=数据
- 8bit 数据格式，高位在前
- CS 为低时芯片使能，CS 为高时忽略所有信号

### 背光电路（用户手册 4.4 节确认）

模块使用 N-MOSFET（BSS138）驱动背光：
- LED 引脚 → R7(10K) 下拉 → BSS138 栅极
- **LED 高电平** → MOSFET 导通 → LEDK 接地 → **背光点亮**
- **LED 低电平** → MOSFET 截止 → 背光熄灭
- 悬空会导致不稳定，**必须配置为确定的电平**
- PWM 调光也通过 LED 引脚输入

### FT6336G I2C 协议（数据手册确认）

| 参数 | 值 |
|------|-----|
| I2C 从机地址 | **0x38**（7 位） |
| SCL 频率 | 10～400 kHz |
| 工作电压 | 2.8～3.6V |
| INT 信号 | 低电平有效（有触摸时通知主机） |
| RST 信号 | 低电平复位，脉冲 > 5ms |
| 上电就绪时间 | **300ms** |
| 默认报点率 | 60Hz（Active 模式） |
| 触摸点数 | 1 点 + 手势 / 2 点 |

---

## 引脚分配

所有新增引脚均位于 **GPIOB**，当前无任何占用：

### SPI LCD（SPI2, AF5, APB1 @ 45MHz）

| 引脚 | 信号 | MSP2834 引脚 | 方向 | 复用 | 说明 |
|------|------|-------------|------|------|------|
| PB13 | SCK | 7-SCK | OUT | AF5 (SPI2_SCK) | 串行时钟 |
| PB15 | MOSI | 6-SDI | OUT | AF5 (SPI2_MOSI) | 主出从入 |
| PB14 | MISO | 9-SDO | IN | AF5 (SPI2_MISO) | 可选回读 |
| PB12 | CS | 3-LCD_CS | OUT | 软件 NSS | 片选（低有效） |
| PB10 | DC | 5-LCD_RS | OUT | GPIO | 数据/命令选择 |
| PB11 | RST | 4-LCD_RST | OUT | GPIO | LCD 复位（低有效） |
| PB0 | BL | 8-LED | OUT | GPIO | 背光（高=ON） |

### I2C 触摸（I2C1, AF4, APB1, 400kHz Fast Mode）

| 引脚 | 信号 | MSP2834 引脚 | 方向 | 复用 | 说明 |
|------|------|-------------|------|------|------|
| PB6 | SCL | 10-CTP_SCL | OUT | AF4 (I2C1_SCL) | 400kHz 时钟 |
| PB7 | SDA | 12-CTP_SDA | BIDIR | AF4 (I2C1_SDA) | 开漏，模块自带上拉 |
| PB8 | INT | 13-CTP_INT | IN | EXTI8 下降沿 | 触摸中断（低有效） |
| PB9 | RST | 11-CTP_RST | OUT | GPIO | FT6336G 复位 |

---

## FT6336G 寄存器映射与应用

### 触摸数据读取（从 0x02 开始连续读 16 字节）

| 地址 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | Mode_Switch | 写 0x00 切换到正常模式 |
| 0x02 | TD_STATUS | 当前报点个数（0～2） |
| 0x03 | P1_XH | bit7-6: 事件标志, bit3-0: X[11:8] |
| 0x04 | P1_XL | X[7:0] |
| 0x05 | P1_YH | bit7-4: 触摸 ID, bit3-0: Y[11:8] |
| 0x06 | P1_YL | Y[7:0] |
| 0x07 | P1_WEIGHT | 触摸压力/面积 |
| 0x08 | P1_MISC | 其他信息 |
| 0x09 | P2_XH | 第 2 点（格式同第 1 点） |
| 0x0A | P2_XL | |
| 0x0B | P2_YH | |
| 0x0C | P2_YL | |
| 0x0D | P2_WEIGHT | |
| 0x0E | P2_MISC | |

### 事件标志（P1_XH[7:6]）

| bit[7:6] | 含义 |
|----------|------|
| 00 | 按下（Press Down） |
| 01 | 抬起（Lift Up） |
| 10 | 持续触摸（Contact） |
| 11 | 保留 |

### 坐标计算

```
X = ((P1_XH & 0x0F) << 8) | P1_XL   // 12bit, 0~239
Y = ((P1_YH & 0x0F) << 8) | P1_YL   // 12bit, 0~319
```

### 芯片识别

| 地址 | 寄存器 | FT6336G 典型值 |
|------|--------|---------------|
| 0xA3 | ID_G_CIPHER_HIGH | 0x64 |
| 0x9F | ID_G_CIPHER_MID | 0x26 |
| 0xA0 | ID_G_CIPHER_LOW | 0x01（0x01=FT6336G） |
| 0xA8 | ID_G_FOCALTECH_ID | 0x11 |

---

## ILI9341 关键命令速查

| 命令 | 代码 | 参数 | 说明 |
|------|------|------|------|
| Software Reset | 0x01 | 无 | 软件复位，复位后需 120ms |
| Sleep Out | 0x11 | 无 | 退出休眠，需 120ms 延迟 |
| Display ON | 0x29 | 无 | 开启显示 |
| Display OFF | 0x28 | 无 | 关闭显示 |
| Column Address Set | 0x2A | 4 bytes | X 起始/结束 16bit |
| Page Address Set | 0x2B | 4 bytes | Y 起始/结束 16bit |
| Memory Write | 0x2C | N bytes | 写入像素数据（RGB565） |
| Memory Read | 0x2E | N bytes | 读取像素数据 |
| Pixel Format Set | 0x3A | 1 byte | **0x55** = 16bit/pixel RGB565 |
| Memory Access Control | 0x36 | 1 byte | **0x08** = BGR 顺序，竖屏 |
| Interface Control | 0xF6 | 3 bytes | 接口模式控制 |
| Power Control 1 | 0xC0 | 1 byte | VRH[5:0] |
| Power Control 2 | 0xC1 | 1 byte | SAP[2:0]; BT[3:0] |
| Frame Rate Control | 0xB1 | 2 bytes | 帧率控制 |
| Display Inversion | 0x21 | 无 | 显示反转 ON |
| Gamma Set | 0x26 | 1 byte | Gamma 曲线选择 |
| Positive Gamma | 0xE0 | 15 bytes | 正 Gamma 校正 |
| Negative Gamma | 0xE1 | 15 bytes | 负 Gamma 校正 |

完整初始化序列见 `docs/schematic/MSP2834/MSP2834/ILI9341V_Init.txt`。

---

## 新增文件清单

### BSP 驱动层

| 文件 | 用途 |
|------|------|
| `app/bsp_spi_lcd.h` | SPI LCD 引脚定义、初始化原型、命令原语声明 |
| `app/bsp_spi_lcd.c` | SPI2 初始化 + ILI9341 复位 + 初始化序列 + WriteCmd/WriteData/WriteData16 |
| `app/bsp_i2c_touch.h` | FT6336G 引脚定义、寄存器地址宏、触摸数据结构声明 |
| `app/bsp_i2c_touch.c` | I2C1 初始化 + FT6336G 读写 + 坐标解析 + EXTI8 中断 |

### LVGL 移植层

| 文件 | 用途 |
|------|------|
| `components/lvgl_port.h` | 移植配置宏、init/flush/touch 函数原型 |
| `components/lvgl_port_disp.c` | 双缓冲 + `lv_display_flush_cb`（SPI 逐行发送像素） |
| `components/lvgl_port_touch.c` | `lv_indev_read_cb`（从 FT6336G I2C 寄存器读取坐标） |

### LVGL 配置

| 文件 | 用途 |
|------|------|
| `components/lv_conf.h` | RGB565、240×320、双缓冲 28.8KB、禁用 GPU/FS |

### 任务层

| 文件 | 用途 |
|------|------|
| `task/task_display.h` | 任务原型声明 |
| `task/task_display.c` | FreeRTOS 显示任务：5ms 周期，`lv_tick_inc(5)` + `lv_timer_handler()` |

### LVGL 库文件

将 LVGL v8.x 源码复制到 `third_lib/lvgl/`，选择性编译核心文件。

---

## 需修改的现有文件

### 1. `task/task_entry.c` — 核心变更

在 `Task_Entry_All()` 中新增初始化调用：

```c
BSP_SPI_LCD_Init();       // SPI2 + ILI9341 复位 + 初始化序列
BSP_I2C_Touch_Init();     // I2C1 + FT6336G 复位 + 芯片 ID 验证
LVGL_Port_Init();         // LVGL 缓冲 + 显示/触摸驱动注册
// ...
xTaskCreate(Task_Display, "DISPLAY", 1024, NULL, 2, &xDisplayTaskHandle);
```

### 2. `mdk/app.uvprojx` + `mdk/app_b.uvprojx` — 工程配置

**firmware/driver 组新增 SPL 源文件：**
- `stm32f4xx_spi.c`
- `stm32f4xx_i2c.c`
- `stm32f4xx_dma.c`

**bsp 组新增：**
- `bsp_spi_lcd.c`
- `bsp_i2c_touch.c`

**components 组新增：**
- `lvgl_port_disp.c`
- `lvgl_port_touch.c`
- `lv_conf.h`（include path）

**task 组新增：**
- `task_display.c`

**新建 lvgl 组**，添加约 30+ LVGL 核心源文件。

**Include Paths 追加：**
```
..\third_lib\lvgl;..\third_lib\lvgl\src;..\components
```

两个 Target（`stm32f429` 和 `stm32f429_b`）需**同步修改**。

### 3. `firmware/cmsis/device/stm32f4xx_it.c` — 新增中断

- `EXTI8_IRQHandler`（FT6336G 触摸中断，NVIC 优先级 12）
- `I2C1_EV_IRQHandler` / `I2C1_ER_IRQHandler`（初始空桩，调试时填充）

---

## ILI9341 初始化序列（参考 ILI9341V_Init.txt）

核心步骤：
1. **硬件复位**：RST=HIGH → 50ms → RST=LOW → 100ms → RST=HIGH → 50ms
2. **电源配置**（0xCF, 0xED, 0xE8, 0xCB, 0xF7, 0xEA）
3. **电源控制**（0xC0, 0xC1, 0xC5, 0xC7）
4. **Display Inversion ON**（0x21）
5. **Memory Access Control**（0x36 = 0x08，BGR 顺序）
6. **Display Function**（0xB6）
7. **Pixel Format**（0x3A = 0x55，16bit RGB565）
8. **Interface Control**（0xF6）
9. **Frame Rate**（0xB1）
10. **Gamma**（0xE0, 0xE1）
11. **Sleep Out**（0x11）→ delay 120ms
12. **Display ON**（0x29）

---

## 任务架构

新增 1 个 FreeRTOS 任务：

| 任务名 | 栈 | 优先级 | 周期 | 职责 |
|--------|-----|--------|------|------|
| DISPLAY | 1024 字 (4KB) | **2** | 5ms | LVGL tick、渲染刷新、触摸数据读取 |

优先级说明：
- 3: CAN_TX / CAN_RX / CAN_TEST（通信优先）
- 2: DISPLAY、KEY_SCAN、UART_QUERY
- 1: HEARTBEAT

LVGL 渲染不抢占 CAN 通信，UI 响应性与按键/串口同级。

---

## 内存与缓冲

```
双缓冲：2 × (240 × 30 像素) × 2 byte(RGB565) = 28.8 KB
占用主 SRAM (0x20000000)，全局静态分配，禁止 CCM
```

- 绘制缓冲 28.8KB ↔ 主 SRAM 192KB 占比约 15%，余量充足
- LVGL 动态对象分配在 CCM 堆，预计 10-20KB
- Flash 增量约 60-90KB，384KB 槽余量充足
- OTA 升级使用内部 Flash 真 AB 分区，**不依赖外挂 Flash**

---

## 实施步骤

### 第 1 步：SPL 库集成 + BSP 驱动
1. 将 `spi.c`、`i2c.c`、`dma.c` 加入 Keil 工程
2. 创建 `bsp_spi_lcd.c/h`：SPI2 初始化（SPI 模式 0、8bit、9MHz）+ ILI9341 初始化序列 + WriteCmd/WriteData/SetWindow
3. 创建 `bsp_i2c_touch.c/h`：I2C1 初始化（400kHz Fast Mode）+ FT6336G 寄存器读写 + 坐标解析
4. 在 `stm32f4xx_it.c` 中添加 EXTI8/I2C ISR 空桩
5. 编译验证（0 error 0 warning）

### 第 2 步：LVGL 集成
1. 复制 LVGL v8.x 源码到 `third_lib/lvgl/`
2. 创建 `lv_conf.h`（RGB565、240×320、双缓冲、无 GPU、禁文件系统）
3. 创建 `lvgl_port_disp.c`（双缓冲 + SPI flush 回调，逐区域 DMA/SPI 发送）
4. 创建 `lvgl_port_touch.c`（FT6336G indev 回调，读取 0x02-0x06 坐标寄存器）
5. 将 LVGL 源文件和移植文件加入 Keil 工程
6. 编译验证

### 第 3 步：显示任务 + 入口集成
1. 创建 `task_display.c`（5ms 周期，`lv_tick_inc(5)` + `lv_timer_handler()`）
2. 修改 `task_entry.c`：新增 BSP 初始化调用 + DISPLAY 任务创建
3. 完整编译 + 链路验证

### 第 4 步：集成测试
1. 测试 UI（全屏纯色 → label "Hello" → arc 控件），验证显示
2. 通过串口 printf 打印触摸坐标，验证 I2C 通信
3. LVGL demo 验证（label + arc + btn 交互）
4. CAN 数据接入 UI（电机状态帧 → 仪表盘控件）
5. FreeRTOS 稳定性（30 分钟无 HardFault，栈高水位 >30%）

---

## 不影响的部分

- **CAN 协议**：无需变更
- **时钟配置**：无需变更（SYSCLK 180MHz, APB1 45MHz, APB2 90MHz）
- **OTA/分区**：无需变更（内部 Flash 真 AB 分区，无外挂 Flash）
- **其他任务**：无需变更
- **SD 卡槽**：SPI 总线共享，SD_CS 悬空不使能即可

---

## 数据手册索引

| 文档 | 路径 |
|------|------|
| 用户手册 | `docs/schematic/MSP2834/MSP2834/2.8inch_SPI_Module_MSP2833_MSP2834_User_Manual_CN.pdf` |
| 产品规格书 | `docs/schematic/MSP2834/MSP2834/MSP2833_MSP2834_规格书_CN_V1.0.pdf` |
| 原理图 | `docs/schematic/MSP2834/MSP2834/2.8inch_SPI_Module_MSP2833_MSP2834_Schematic.pdf` |
| ILI9341 数据手册 | `docs/schematic/MSP2834/MSP2834/ILI9341_Datasheet.pdf` |
| ILI9341 初始化序列 | `docs/schematic/MSP2834/MSP2834/ILI9341V_Init.txt` |
| FT6336G 数据手册 | `docs/schematic/MSP2834/MSP2834/D-FT6336G-DataSheet-V1.0.pdf` |
| FT6336G 寄存器表 | `docs/schematic/MSP2834/MSP2834/FT6336G_Register.xlsx` |
| MSP2834 尺寸图 | `docs/schematic/MSP2834/MSP2834/2.8inch_SPI_Module_MSP2833_Size.pdf` |
| 触摸尺寸图 | `docs/schematic/MSP2834/MSP2834/2.8inch_CTP_Module_FT6336G_Size.pdf` |
| 官网参考 | http://www.lcdwiki.com/zh/2.8inch_IPS_SPI_Module_ILI9341 |

---

## 验证方法

### 编译验证
- 两个 Target（`stm32f429` + `stm32f429_b`）均 0 error 0 warning
- Arm Compiler 5 (armcc V5.06) + C99

### 功能验证
1. LCD 上电后 SPI2 正常输出时钟，LCD 显示纯色（红/绿/蓝）
2. I2C1 读取 FT6336G 芯片 ID（0xA3=0x64, 0xA0=0x01），确认通信
3. 触摸后 printf 打印坐标，验证 I2C 通信正常
4. LVGL 测试 UI 正常显示（label 文字、arc 动画）
5. 触摸交互可改变 UI 状态

### 稳定性验证
- 运行 30 分钟无 HardFault
- FreeRTOS 栈高水位线 > 30%
- 无 CAN 帧丢失
