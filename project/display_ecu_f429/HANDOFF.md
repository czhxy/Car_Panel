# HANDOFF.md — 显示域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 模块完成度总表

| 模块 | 完成度 | 文件/位置 | 说明 |
|---|---|---|---|
| Bootloader (YMODEM OTA) | 100% | `bootloader/` | YMODEM-1K + 真 AB 分区 + CRC32 校验 |
| OTA 参数管理 | 100% | `bootloader/ota_params.c` | Sector 4，append-only 日志，1024 槽 × 64B，掉电安全 |
| FreeRTOS 集成 | 100% | `third_lib/FreeRTOS` | v11.3.0，heap_4 @ CCM，7 任务运行 |
| CAN 通信框架 | 100% | `task/mod_comm_can.c/h` | TX/RX 双 FreeRTOS 队列，ISR 接收，心跳，测试帧 |
| CAN 协议层 | 90% | `task/task_comm_can_protocol.c/h`, `protocol/CAN_Protocol.h` | ID 编解码完成，电机控制帧周期发送，调试查询占位 |
| 串口日志 | 100% | `app/bsp_log.h`, `driver/usart.c/h` | printf 重定向，`LOG_E/W/I/D` 宏，环形缓冲 RX |
| 按键驱动 | 100% | `app/bsp_key.c/h` | 20ms 扫描，上升沿检测，FreeRTOS 信号量通知 |
| LED 驱动 | 100% | `app/bsp_led.c/h` | 4 路 GPIO 输出 |
| UART 查询服务 | 100% | `task/task_query.c/h` | `0xAA 0x55` 协议，芯片信息/VTOR 自证槽位查询 |
| SPI LCD 驱动 (ILI9341V) | **100%** | `app/bsp_spi_lcd.c/h` + `app/bsp_spi_lcd_font.h` | SPI5 SPL 4 线接口，240×320 RGB565，ILI9341 全初始化序列 |
| I2C 触摸驱动 (FT6336G) | **100%** | `app/bsp_i2c_touch.c/h` | I2C1 SPL 400kHz，2 点触摸，ID 验证 |
| GUI 绘图库 | **100%** | `task/mod_gui.c/h` | 点/线/圆/矩形/三角形/字符/图片绘制 |
| LCD 演示与触摸测试 | **100%** | `task/mod_test.c/h` + `task/task_lcd_demo.c/h` | 10 项综合测试 + 触摸坐标验证 |
| 电机传感器驱动 | 5% | `driver/mod_motor.c/h` | 仅占位函数（返回 0.0f） |
| LVGL 仪表盘 UI | 0% | `task/task_display.c`（待建） | 未创建 |

---

## 新建文件清单（本次 LCD 移植）

```
app/
  bsp_spi_lcd.h          SPI5 + ILI9341 接口声明、引脚定义、颜色宏
  bsp_spi_lcd.c          SPI5 SPL 初始化 + ILI9341 60+ 寄存器初始化序列
  bsp_spi_lcd_font.h     ASCII 6x12/8x16 + 中文 16/24/32 字库点阵
  bsp_i2c_touch.h        I2C1 + FT6336G 寄存器定义、触摸结构体
  bsp_i2c_touch.c        I2C1 SPL 400kHz + FT6336G 驱动 + ID 验证
task/
  mod_gui.h              GUI 绘图库接口
  mod_gui.c              点/线/圆/矩形/三角/字符/图片 绘制实现
  mod_test.h             测试函数声明
  mod_test.c             10 项综合测试实现（含触摸测试）
  mod_test_pic.h         40×40 QQ 图片取模数据
  task_lcd_demo.h        演示任务声明
  task_lcd_demo.c        循环演示任务（每项间隔 2s）
```

---

## LCD/TOUCH 引脚分配（零冲突）

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| SPI5_SCK | PF7 | SPI5 | 空闲 |
| SPI5_MISO | PF8 | SPI5 | 空闲 |
| SPI5_MOSI | PF9 | SPI5 | 空闲 |
| LCD_RS | PI8 | GPIO | 命令/数据选择 |
| LCD_RST | PI9 | GPIO | 硬件复位 |
| LCD_CS | PI10 | GPIO | 片选 |
| LCD_BL | PD6 | GPIO | 背光控制（高亮） |
| I2C1_SCL | PB6 | I2C1 | 触摸 I2C 时钟 |
| I2C1_SDA | PB7 | I2C1 | 触摸 I2C 数据 |
| CTP_INT | PB8 | GPIO | 触摸中断（输入上拉） |
| CTP_RST | PB9 | GPIO | 触摸复位（推挽输出） |

---

## 数据流（当前完整运行的路径）

```
┌────────────────────────────────────────────────────────────┐
│ 硬件层                                                     │
│  CAN FIFO0 ISR → Mod_Can_RxIRQHandler() → CanRxQueue       │
│  Systick ISR → FreeRTOS tick                               │
│  USART1 IRQ → driver/usart.c 环形缓冲 RX                    │
│  KEY ISR → GPIO 扫描 → xSemaphoreGive                      │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│ FreeRTOS 任务                                               │
│                                                            │
│  Mod_Can_TxTask (prio 4, 1ms):                              │
│    → ModCommCan_Tx() 出 TX 队列 → CAN_Transmit()            │
│    → CanProtocol_WheelCtlSend() → 10ms 限频电机控制帧        │
│                                                            │
│  Mod_Can_RxTask (prio 4):                                  │
│    → xQueueReceive(RxQueue) → ModCommCan_OnRxFrame()        │
│                                                            │
│  CAN_Test_Task (prio 4):                                   │
│    → 等待 xKey1Sem → Mod_Can_TxTest()                       │
│                                                            │
│  KEY_SCAN (prio 2, 20ms):                                   │
│    → 按键扫描 → xSemaphoreGive                              │
│                                                            │
│  UART_QUERY (prio 2):                                      │
│    → 状态机解析 0xAA 0x55 协议 → 查询应答                     │
│                                                            │
│  LCD_DEMO (prio 3):                                        │
│    → 循环运行测试项 → 触摸坐标实时显示                        │
│    → Touch_Test() 8s 触摸验证 → log 输出结果                 │
│                                                            │
│  Heartbeat_Task (prio 1, 500ms):                            │
│    → LED 翻转 + Can_Heartbeat() → Mod_Can_TxEvent()         │
└────────────────────────────────────────────────────────────┘
```

---

## Task_Entry_All 初始化顺序

```
---- BSP 硬件初始化 ----
BSP_LED_Init()
BSP_KEY_Init()
Mod_Can_Init()          ← 先创建 CAN 队列，再使能硬件中断
BSP_CAN_Init()
BSP_SPI_LCD_Init()      ← SPI5 + ILI9341 (vTaskDelay 复位时序)
BSP_I2C_Touch_Init()    ← I2C1 + FT6336G (ID 验证)

---- 创建 FreeRTOS 任务 ----
CAN_TX (512, prio 4)
CAN_RX (512, prio 4)
CAN_TEST (256, prio 4)
KEY_SCAN (256, prio 2)
HEARTBEAT (512, prio 1)
UART_QUERY (256, prio 2)
LCD_DEMO (512, prio 3)
```

---

## 关键设计决策

1. **SPI 外设选 SPI5**：PF7/8/9 引脚，与现有外设零冲突。SPI 模式 3（CPOL=1, CPHA=1），波特率预分频 2（45MHz）

2. **触摸 I2C 用硬件 SPL**：I2C1 400kHz Fast Mode，替代原 bit-bang GPIO 方式；复位后验证 4 个芯片 ID（0xA8=0x11, 0x9F=0x26, 0xA3=0x64）

3. **LCD_Clear 用寄存器直接写入**：绕过 `SPI_WriteByte()` 函数调用开销，直接用 `SPI5->DR` 写入并轮询标志位

4. **中文字库以占位符编译**：原始 FONT.H 中的 GBK 编码字符与 ARMCC V5 不兼容，字库索引替换为 `"__"` 占位（中文测试页未使用实际中文字符）

5. **FreeRTOS 队列 vs 环形队列**：TX/RX 使用 FreeRTOS 队列（线程安全、挂起等待），`components/my_queue.c` 为备用环形队列（当前未被 App 使用）

6. **CAN TX 邮箱保护**：`ModCommCan_Tx()` 邮箱满时用 `xQueueSendToFront` 回灌队首后 break（不丢数据，等待下次出队）

7. **弱符号回调**：`ModCommCan_OnRxFrame()` 为 `__weak` 弱符号，应用层可定义同名强符号覆盖默认行为

8. **CAN 滤波器全通**：掩码=0，接收总线所有 29-bit 扩展帧，后续可按源地址滤波器细化

---

## 当前编译状态

- **App 工程**：`mdk/app.uvprojx`，双 Target（stm32f429 / stm32f429_b），armcc V5.06, C99
- **Bootloader 工程**：`mdk/boot.uvprojx`，独立编译
- **新增 SPL 源文件**：`stm32f4xx_spi.c`、`stm32f4xx_i2c.c` 已加入 firmware/driver 组
- App A 槽 → 0x08020000，App B 槽 → 0x08080000

---

## 下一步待做工作（按优先级）

### P1 — LVGL 仪表盘 UI（`task/task_display.c`）

依赖 P0 LCD/触摸驱动已完成：

- LVGL v8.x 移植：双缓冲（2×240×30×2 = 28.8KB 主 SRAM）、SPI flush 回调、FT6336G indev 回调
- 仪表盘界面渲染（速度表盘、方向指示灯、电量/油量等）
- 创建 LVGL 渲染任务（单独 FreeRTOS 任务，5ms 周期）
- 数据驱动：将 CAN 接收到的电机状态映射到 LVGL 控件

### P2 — 电机传感器真实数据接入

将 `driver/mod_motor.c` 的占位函数替换为实际实现：
- 当前 `Mod_Motor_Get_Speed()` 和 `Mod_Motor_Angle()` 均硬编码返回 0.0f
- 需要对接真实传感器数据源（通过 CAN 从动力域获取后写入）

### P3 — 动力域 CAN 协议帧补齐

当前向动力域发送的电机控制帧数据字段为 0（因为传感器返回 0.0f），需要：
- 实现有意义的电机目标转速/电流设置
- 接收和解析动力域上报的电机状态帧、心跳帧
- 在 `ModCommCan_OnRxFrame()` 强符号实现中处理

### P4 — CAN 滤波器细化（`app/bsp_can.c`）

当前全通（掩码=0），需要按源地址过滤：
- 只接收来自 `CAN_ADDR_MOTORBOARD` (0x02) 的帧
- 减少中断负载

---

## 已知注意事项

1. **CCM 与 DMA**：FreeRTOS 堆在 CCM（`0x10000000`），DMA 无法访问。任何 DMA 缓冲必须放在 `.bss`/全局数组（落在 `0x20000000` 主 SRAM）

2. **`#include "can_protocol.h"` 大小写**：`mod_comm_can.h:8` include `"can_protocol.h"`，在 Windows（大小写不敏感）下能匹配 `protocol/CAN_Protocol.h`。如果迁移到 Linux CI 构建须改为 `"CAN_Protocol.h"`

3. **Delay_us/ms 与 FreeRTOS**：`driver/Delay.c` 直接操作 SysTick 寄存器，与 FreeRTOS 的 SysTick 中断冲突。App 内应使用 `vTaskDelay()`，不可调用 `Delay_us()/Delay_ms()`

4. **SPI 速度**：ILI9341 初始化后 LCD_Clear 用直接寄存器访问方式，`SPI_WriteByte()` 用于普通操作，读操作时降速至预分频 16

5. **触摸 ID 验证**：`FT6336_Init()` 需验证 VENDOR(0xA8=0x11)、MID(0x9F=0x26)、HIGH(0xA3=0x64)，任一失败返回 1

6. **AB 分区调试**：编译 B 槽 Target 时，如果要将 B 镜像烧入 A 槽地址调试，需临时切换 scatter 起始地址
