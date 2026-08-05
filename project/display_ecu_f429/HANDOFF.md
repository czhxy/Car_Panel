# HANDOFF.md — 显示域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 模块完成度总表

| 模块 | 完成度 | 文件/位置 | 说明 |
|---|---|---|---|
| Bootloader (YMODEM OTA) | 100% | `bootloader/` | YMODEM-1K + 真 AB 分区 + CRC32 校验 |
| OTA 参数管理 | 100% | `bootloader/ota_params.c` | Sector 4，append-only 日志，1024 槽 × 64B，掉电安全 |
| FreeRTOS 集成 | 100% | `third_lib/FreeRTOS` | v11.3.0，heap_4 @ CCM，8 任务运行 |
| CAN 通信框架 | 100% | `task/mod_comm_can.c/h` | TX/RX 双 FreeRTOS 队列，ISR 接收，心跳，测试帧 |
| CAN 协议层 | 90% | `task/task_comm_can_protocol.c/h`, `protocol/CAN_Protocol.h` | ID 编解码完成，电机控制帧周期发送，调试查询占位 |
| 串口日志 | 100% | `app/bsp_log.h`, `driver/usart.c/h` | printf 重定向，`LOG_E/W/I/D` 宏，纯硬件原语（App 无 ISR/环形缓冲，USART1 ISR 在 stm32f4xx_it.c） |
| UART 通信框架 | **100%** | `task/mod_comm_uart.c/h` | 仿 CAN：字节队列(256) + TX 包队列(8) + `UART_TX`/`UART_RX` 双任务 + 弱符号回调，0xAA 0x55 拼包/组包/CRC16，环路验证 2/2 PASS |
| 按键驱动 | 100% | `app/bsp_key.c/h` | 20ms 扫描，上升沿检测，FreeRTOS 信号量通知 |
| LED 驱动 | 100% | `app/bsp_led.c/h` | 4 路 GPIO 输出 |
| UART 查询服务 | 100% | `task/task_query.c/h` | 强符号覆盖 `ModCommUart_OnRxPacket`：芯片信息/VTOR 自证槽位查询 + 其他 type 回环 echo |
| SPI LCD 驱动 (ILI9341V) | **100%** | `app/bsp_spi_lcd.c/h` + `app/bsp_spi_lcd_font.h` | SPI5 SPL 4 线接口，240×320 RGB565，ILI9341 全初始化序列 |
| I2C 触摸驱动 (FT6336G) | **100%** | `app/bsp_i2c_touch.c/h` | I2C1 SPL 400kHz，2 点触摸，ID 验证 |
| GUI 绘图库 | **100%** | `task/mod_gui.c/h` | 点/线/圆/矩形/三角形/字符/图片绘制（LVGL 接管后保留备用） |
| LCD 演示与触摸测试 | **100%** | `task/mod_test.c/h` | 10 项综合测试 + 触摸坐标验证（LVGL 接管后保留备用） |
| **LVGL 移植与 Demo** | **100%** | `third_lib/LVGL/` + `task/task_lcd_demo.c` | v8.3.11，DMA2D GPU 加速，双缓冲，Widgets Demo 跑通 |
| 电机传感器驱动 | 5% | `driver/mod_motor.c/h` | 仅占位函数（返回 0.0f） |
| **LVGL 仪表盘 UI** | **96%** | `task/task_dashboard_ui.c/h` + `task/mod_dashboard_data.c/h` | 已替换仪表盘资源、隐藏指定图标和错误码；RPM 表盘由拖动条控制（0–100 ×3 → 0–300），拖动保留 CAN 下发；**移除 Top Bar 图片**，CAN 指示灯改用 LVGL 原生控件（6×6 红/绿圆点 + lv_label 文本框，**始终闪烁**：在线绿 / 离线红，500ms 周期）；**隐藏 0/50/100 刻度数字**；新增**红色 PAUSE 一键暂停按钮**（切换式：暂停归 0 锁滑块 / 恢复原值） |
| VOFA+ 调试输出（临时） | **调试用** | `task/task_vofa.c/h` + `driver/usart6.c/h` | USART6 (PC6/PC7) 每 100ms 输出 rpm_target，firewater ASCII 格式；测试完成后整段移除（`VOFA_DEBUG` 开关） |

---

## 新建文件清单

### 本次新增 — UART 通信框架（查询链路拆分）

```
task/
  mod_comm_uart.h    UART 通信框架头文件：队列深度/帧格式/API/弱符号回调声明
  mod_comm_uart.c    UART 通信框架实现（仿 mod_comm_can）：
                       RX: USART1 ISR → 字节队列(256) → UART_RX_Task 拼包
                           → ModCommUart_OnRxPacket() [弱符号]
                       TX: Mod_Uart_SendPacket() 组包入 TX 队列(8)
                           → UART_TX_Task 临界区消费 → UART_SendArray()
                       帧格式: [0xAA][0x55][type][len][data][crc16_hi][crc16_lo]
                       len==0 帧兼容 PC 旧查询指令（无 CRC），len>0 严格 CRC 校验

tools/
  uart_loopback_test.py  环路验证脚本：
                          测试1 chip info 查询 → 应答 [AA 55 01 0D <13B> <crc>]
                          测试2 回环 echo → [AA 55 10 04 01 02 03 04 <crc>] 原样回发
                          实测 2 PASS / 0 FAIL
```

**配合修改**：
- `driver/usart.c/h`：移除 App 用 ISR + 环形缓冲 + `UART_RxGet`，保留纯硬件原语
- `firmware/cmsis/device/stm32f4xx_it.c/h`：新增 `USART1_IRQHandler` 转发到 `Mod_Uart_RxIRQHandler()`（与 `CAN1_RX0_IRQHandler` 对齐）
- `task/task_query.c/h`：删除 `UART_Query_Task` 任务主体与"透传 CAN"脚手架，改为覆盖回调
- `task/task_entry.c`：`Mod_Uart_Init()` + `UART_TX`/`UART_RX` 任务（prio 4, 256 字）
- `mdk/app.uvprojx` / `app_b.uvprojx`：task 分组新增 `mod_comm_uart.c/h`

### 本次新增 — 仪表盘 UI

```
tools/
  bin_to_c_array.py       一次性地将 pic/*.bin → C 数组 + lv_img_dsc_t

pic/
  dashboard_images.h      当前使用图片的 extern lv_img_dsc_t 声明
  dashboard_images.c      当前使用图片的 const uint8_t[] 像素数据 + 描述符
                          Top Bar、arc-bg、arc-fill、CAN 状态点、CAN 文本、
                          Turn Signals 和 mode；旧图片仍保留在 pic/，但不会被转换

task/
  mod_dashboard_data.h    DashboardState 结构体 + DashboardCard 枚举 +
                          FreeRTOS 互斥锁 API 声明
  mod_dashboard_data.c    全局状态初始化、互斥锁创建、快照读取实现
  mod_dashboard_fault.h   FaultCodeEntry 结构体 + FaultLevel 枚举 + 查询 API
  mod_dashboard_fault.c   故障码→消息映射表（4 条初始映射，可扩展追加）
  task_dashboard_ui.h     Dashboard_UI_Init() + Dashboard_Update() 声明
  task_dashboard_ui.c     全部 UI 元素构建（~350 行） + 25ms 周期更新逻辑
```

### 本次新增 — LVGL 仪表盘改进 + VOFA 调试（2026-08-05）

```
task/
  task_vofa.h           VOFA 调试总开关 VOFA_DEBUG + Vofa_Task 声明
  task_vofa.c           Vofa_Task：每 100ms 读 g_dash_state.rpm_target，
                        USART6 以 firewater ASCII 格式发送 "%u\r\n"（临时，测试后删）

driver/
  usart6.h              USART6 发送原语声明
  usart6.c              USART6 (PC6/PC7, AF8, APB2, 115200) 初始化 + SendByte/SendString
                        仅发送无接收，专供 VOFA+（临时，测试后删）
```

**配合修改**：
- `task/task_dashboard_ui.c`：
  - **移除 Top Bar 图片，CAN 指示灯改用 LVGL 原生控件**：不再创建 `img_top_bar` 背景图（其内嵌静态状态点无论遮挡还是对齐都不可靠，曾导致"绿色常亮不闪烁"）。改为 6×6 圆形 `lv_obj`（在线绿色闪烁 / 离线红色常亮，直接改 `bg_color`/`bg_opa`）+ `lv_label` "CAN" 文本框，完全不依赖图片像素
  - **RPM 由拖动条控制**：表盘数值改显示 `snap.rpm_target`（0–100 ×3 → 0–300），不再显示 CAN 实测 `snap.rpm`；`on_load_change` 保留 CAN 目标转速下发
  - **隐藏刻度 + PAUSE 一键暂停**：注释掉 0/50/100 三个刻度 `lv_label`；仪表盘与 Load Bar 间新增红色圆角矩形按钮（80×30, y=200），切换式暂停——按下 rpm_target 归 0、滑块回 0 并锁定、发 CAN 0 帧，再按恢复暂停前值。`DashboardState` 新增 `paused` 标志，抽取 `send_rpm_target()` 公共下发函数
- `task/task_entry.c`：`#if VOFA_DEBUG` 条件创建 `VOFA` 任务（256 字, prio 4）
- `mdk/app.uvprojx`（2 Target）+ `app_b.uvprojx`：task 组新增 `task_vofa.c/h`，driver 组新增 `usart6.c/h`

### 历史新增（上次）

详见 git 记录。

---

## 仪表盘 UI 布局 (240×320，深色底 #05080D)

```
y=13   CAN 指示灯（Top Bar 已移除）  ← 6×6 圆形 lv_obj (89,13)
                                     始终 500ms 闪烁：在线绿 / 离线红
       "CAN" 文本框                  ← lv_label，圆点右侧垂直居中
                                       (lv_obj_align_to OUT_RIGHT_MID, gap 4)
y=45   Divider                    ← lv_obj 208×1, rgba(0CCFF,0.08)
y=49   Error Box (150×23)         ← 已隐藏，保留代码在 #if 0 中
y=72   Gauge (120×110, 居中)      ← 容器
         ├ 圆弧背景/填充            ← lv_img(img_arc_bg) + lv_img(img_arc_fill)
         ├ RPM 数值 "6800"         ← lv_label, Montserrat 28
         └ 单位 "RPM"              ← lv_label
y=190  Bottom Cards (208×48)      ← 已注释（代码保留）
y=200  Pause 按钮 (80×30, 居中)   ← 红色圆角矩形 lv_btn + "PAUSE" 标签
         一键暂停: 按下 rpm_target 归 0 + 滑块回 0 锁定; 再按恢复暂停前值
y=240  Load Bar 刻度 (208×12)     ← 已注释（0/50/100 数字按用户要求隐藏）
         目标值: y=253 "126 RPM"  ← lv_label, 拖动后实时变化
         滑块: y=270, 208×20       ← lv_slider (交互型 RPM 下发)
y=292  Turn Signals (208×20)      ← lv_img(img_turn_signals)
         ├ 左箭头点击区 104×20     ← 透明 lv_obj (卡片左移)
         └ 右箭头点击区 104×20     ← 透明 lv_obj (卡片右移)
y=312  Warning Dots (208×8)       ← flex row, 6 个 8×8 圆点
         ABS(黄)/ESC(橙)/引擎(红)/电池(绿)/远光(蓝)/车门(粉)
         电池默认点亮(90%)，其余暗(20%)
         引擎故障时 红色点亮
```

## CAN 数据流（当前完整运行的路径）

```
┌────────────────────────────────────────────────────────────┐
│ 硬件层                                                     │
│  CAN FIFO0 ISR → Mod_Can_RxIRQHandler() → CanRxQueue       │
│  Systick ISR → FreeRTOS tick                               │
│  USART1 ISR → Mod_Uart_RxIRQHandler() → UartRxQueue 字节队列│
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
│    → xQueueReceive(RxQueue)                                 │
│    → ModCommCan_OnRxFrame() [强符号]                        │
│       ├ 心跳帧 (0x320):    → g_dash_state.error_code        │
│       │                      → g_dash_state.motor_online    │
│       └ 电机状态 (0x110):  → g_dash_state.rpm               │
│                            → g_dash_state.odo_value          │
│                            → g_dash_state.motor_status       │
│                                                            │
│  CAN_Test_Task (prio 4):                                   │
│    → 等待 xKey1Sem → Mod_Can_TxTest()                       │
│                                                            │
│  KEY_SCAN (prio 2, 20ms):                                   │
│    → 按键扫描 → xSemaphoreGive                              │
│                                                            │
│  UART_TX (prio 4):                                         │
│    → xQueueReceive(UartTxQueue) [portMAX_DELAY]            │
│    → 临界区 UART_SendArray() 发送（与 printf 互斥）           │
│                                                            │
│  UART_RX (prio 4):                                         │
│    → xQueueReceive(UartRxQueue 字节队列) [100ms]            │
│    → 拼包状态机 0xAA 0x55 type len data crc16              │
│    → ModCommUart_OnRxPacket() [强符号]                      │
│       ├ type=0x01 chip info → 应答 13B（分区/VTOR 自证）      │
│       └ 其他 type → 原样回发（环路验证 echo）                  │
│                                                            │
│  LCD_DEMO (prio 3, 5ms):                                   │
│    → lv_tick_inc() + lv_timer_handler()                     │
│    → Dashboard_UI_Init(scr) 启动时一次性构建                  │
│    → Dashboard_Update() 每 25ms:                            │
│       Dashboard_Data_GetSnapshot() → 读共享状态              │
│       ├ RPM 数值刷新 ← rpm_target（拖动条控制）              │
│       ├ 错误框颜色切换 (红/绿)                               │
│       ├ CAN 指示灯: 在线绿闪(500ms) / 离线红常亮              │
│       ├ 心跳超时检测 (1.5s)                                 │
│       ├ 卡片选择器高亮                                      │
│       └ 警示灯状态更新                                      │
│                                                            │
│  VOFA (prio 4, 100ms):  [VOFA_DEBUG 临时]                  │
│    → 读 g_dash_state.rpm_target                            │
│    → USART6 firewater ASCII: "%u\r\n"                      │
│                                                            │
│  Heartbeat_Task (prio 1, 500ms):                            │
│    → LED 翻转 + Can_Heartbeat() → Mod_Can_TxEvent()         │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│ TX 方向 (显示域 → 动力域):                                   │
│   Load Bar 交互 → g_dash_state.rpm_target 变化              │
│     → CanProto_SendFrame() → CAN TX 队列                    │
│     → CAN 总线 → 动力域 ECU 接收                            │
│   格式: [speed_L, speed_H, 0, 0, 0, 0, 0, 3]               │
└────────────────────────────────────────────────────────────┘
```

## Task_Entry_All 初始化顺序

```
---- BSP 硬件初始化 ----
BSP_LED_Init()
BSP_KEY_Init()
Mod_Can_Init()          ← 先创建 CAN 队列，再使能硬件中断
BSP_CAN_Init()
Mod_Uart_Init()         ← 创建 UART 收发队列（UART_Init 硬件在 main 中已完成）
Query_Task_Init()       ← 查询协议业务初始化（确保 task_query 被链接）
BSP_SPI_LCD_Init()      ← SPI5 + ILI9341 (vTaskDelay 复位时序)
BSP_I2C_Touch_Init()    ← I2C1 + FT6336G (ID 验证)
                          LVGL 由 LCD_DEMO 任务独立初始化（lv_init + port_init +
                          Dashboard_UI_Init）

---- 创建 FreeRTOS 任务 ----
CAN_TX (512, prio 4)
CAN_RX (512, prio 4)
CAN_TEST (256, prio 4)   ← 已注释
KEY_SCAN (256, prio 2)
HEARTBEAT (512, prio 1)
UART_TX (256, prio 4)
UART_RX (256, prio 4)
LCD_DEMO (1024, prio 3)  ← 栈 1024 字 = 4KB，LVGL 渲染开销
VOFA (256, prio 4)       ← [VOFA_DEBUG] USART6 firewater 输出（临时）
```

## 仪表盘线程安全设计

```
CAN_RX Task (prio 4)             LCD_DEMO Task (prio 3)
   │                                  │
   ├─ 写入 g_dash_state              ├─ 读取 g_dash_state
   │  Dashboard_Data_Lock()          │  Dashboard_Data_Lock()
   │  ...修改字段...                  │  memcpy → 本地快照
   │  Dashboard_Data_Unlock()        │  Dashboard_Data_Unlock()
   │                                  │  用快照更新 UI (不加锁)
   │                                  │
   └── FreeRTOS Mutex ────────────────┘
```

- 加锁时间极短（仅拷贝结构体/写几个字段，不阻塞 LVGL 渲染）
- UI 更新使用本地快照，不在锁内操作 LVGL 对象
- 心跳超时检测在 `Dashboard_Update()` 中检查（1.5s 无心跳 → `motor_online = false`）

---

## 故障码映射系统

| 故障码 | 消息 | 级别 | 说明 |
|---|---|---|---|
| `0x0000` | "ALL SYSTEMS NORMAL" | NONE | 无故障 |
| `0x0001` | "CAN TIMEOUT" | ERROR | CAN 心跳超时 |
| `0x0002` | "MOTOR STALL" | ERROR | 电机堵转 |
| `0x0004` | "ENCODER LOSS" | ERROR | 编码器丢失 |

扩展方法：在 `mod_dashboard_fault.c` 的 `s_fault_table[]` 数组中追加一行：
```c
{ 0x0008, "OVERCURRENT", FAULT_ERROR },
```

## 错误框颜色切换

| 状态 | 背景 | 边框 | 图标 | 文字 | 错误码 |
|---|---|---|---|---|---|
| 正常(无故障) | `#020A04` | `#33CC4D` 12% | `#33CC4D` | `#33CC4D` 80% | `"OK"` `#33CC4D` |
| 故障 | `#0A0202` | `#F54236` 12% | `#F54236` | `#F54236` 80% | `"E002"` `#2A2A2A` |

---

## 图片资源使用决策

| 图片 | 使用方式 | 原因 |
|---|---|---|
| Top Bar.bin | **不使用** | 内嵌静态 CAN 状态点无法受控（曾致"绿色常亮不闪烁"）；CAN 指示灯改用 LVGL 原生圆点 + 文本框 |
| arc-bg.bin + arc-fill.bin | `lv_img` 叠加 | 替换 Frame.bin；RPM 数字使用 LVGL 标签实时刷新 |
| ODO/BATTERY/SOC.bin | **不使用** | 仅保留卡片和实时数值标签，避免显示图标 |
| can-dot_green.bin + can-dot_red.bin | **不使用** | 已改用 LVGL 原生圆形 `lv_obj`（改 `bg_color` 即可切换红/绿） |
| can-label.bin | **不使用** | 已改用 `lv_label` 文本框 "CAN" |
| Turn Signals.bin | `lv_img` 背景 | 箭头底图，点击区用透明 `lv_obj` 覆盖 |
| mode.bin | `lv_img`（预留） | 档位 "D" 指示器 24×12 |
| Error Box.bin | **不使用** | 改为 LVGL 控件构建（需动态变红/绿色） |
| load-bg/fill/label-*.bin | **不使用** | 改为 `lv_slider` + `lv_label` |

---

## LVGL 配置要点

### lv_conf.h 关键配置

| 配置项 | 值 | 说明 |
|---|---|---|
| `LV_COLOR_DEPTH` | 16 | RGB565，匹配 ILI9341 |
| `LV_COLOR_16_SWAP` | 0 | 不交换字节（SPI MSB 先发，格式正确） |
| `LV_MEM_SIZE` | 48KB | LVGL 内存池，位于主 SRAM (.bss) |
| `LV_TICK_CUSTOM` | 0 | 手动在任务中调用 `lv_tick_inc()` |
| `LV_USE_GPU_STM32_DMA2D` | 1 | DMA2D 硬件加速 |
| `LV_GPU_DMA2D_CMSIS_INCLUDE` | `"stm32f4xx.h"` | DMA2D 驱动的 CMSIS 头文件 |
| `LV_FONT_MONTSERRAT_14` | 1 | 默认字体 |
| `LV_FONT_MONTSERRAT_28` | **1** | 本次新增：RPM 大数字显示 |
| `LV_FONT_DEFAULT` | `&lv_font_montserrat_14` | 默认字体 |
| `LV_USE_DEMO_WIDGETS` | 1 | Widgets Demo（保留但不再调用） |

### 显示缓冲（lv_port_disp.c）

- 双缓冲：2 × 240 × 40 = 19.2KB × 2 = **38.4KB**
- 静态数组，落在主 SRAM (`.bss`)，DMA2D 可访问
- 40 行缓冲为 DMA2D 效率优化（DMA2D block 大小不小于行高时效率最高）

### SRAM 使用估算

| 用途 | 大小 |
|---|---|
| LVGL 内存池 | 48KB |
| 双缓冲 | 38.4KB |
| **合计 LVGL** | **~86KB** |
| 主 SRAM 总量 | 128KB |
| 剩余给 FreeRTOS + 栈 + CAN 队列 | ~42KB |

### Keil 工程 LVGL 分组（118 文件，12 分组）

| 分组 | 文件数 | 变更 | 说明 |
|---|---|---|---|
| `lvgl_core` | 15 | — | LVGL 核心 |
| `lvgl_draw` | 26 | — | 绘图（含 sw 软件渲染 + stm32_dma2d） |
| `lvgl_extra` | 25 | — | 扩展控件、布局、主题 |
| `lvgl_font` | **5** | +1 | Montserrat 14 + **28** 字体 |
| `lvgl_hal` | 3 | — | 硬件抽象层 |
| `lvgl_misc` | 22 | — | 杂项工具箱 |
| `lvgl_widgets` | 15 | — | 基础控件 |
| `lvgl_porting` | 2 | — | 本项目移植文件 |
| `lvgl_config` | 2 | — | lv_conf.h + lvgl.h（FileType=5，不编译） |
| `lvgl_demo_widgets` | 4 | — | Widgets Demo + 图片资源（保留不调用） |
| `lvgl_app` | 0 | — | 预留 |
| `task` | **+6** | +6 | mod_dashboard_data/fault + task_dashboard_ui |
| `pic` | **新增** | +2 | dashboard_images.c/h |

### DMA2D 注意事项

- DMA2D 只能访问主 SRAM (`0x20000000`) 和外设总线，**不能访问 CCM** (`0x10000000`)
- LVGL 的绘制缓冲、`lv_mem` 内存池均为静态分配（`.bss`），自动落在主 SRAM，DMA2D 可直接操作
- 不要在 DMA2D 操作中使用 `pvPortMalloc()` 返回的指针（FreeRTOS 堆在 CCM）
- **图片在 Flash (.rodata)**：LVGL 逐字节读入 SRAM 缓冲后再 DMA2D 绘制（不会直接从 Flash DMA）

---

## 关键设计决策

1. **SPI 外设选 SPI5**：PF7/8/9 引脚，与现有外设零冲突。SPI 模式 3（CPOL=1, CPHA=1），波特率预分频 2（45MHz）

2. **触摸 I2C 用硬件 SPL**：I2C1 400kHz Fast Mode，替代原 bit-bang GPIO 方式；复位后验证 4 个芯片 ID（0xA8=0x11, 0x9F=0x26, 0xA3=0x64）

3. **LCD_Clear 用寄存器直接写入**：绕过 `SPI_WriteByte()` 函数调用开销，直接用 `SPI5->DR` 写入并轮询标志位。LVGL 的 `disp_flush()` 也采用相同高性能方式

4. **LVGL 移植**：v8.3.11，双缓冲 240×40 行（19.2KB × 2），DMA2D GPU 加速，Task_LCD_Demo 每 5ms 调用 `lv_tick_inc()` + `lv_timer_handler()`，使用 `xTaskGetTickCount()` 跟踪时间差

5. **DMA2D Chrom-ART 加速**：`LV_USE_GPU_STM32_DMA2D=1`，`LV_GPU_DMA2D_CMSIS_INCLUDE="stm32f4xx.h"`。DMA2D 负责颜色填充、混合、复制操作，大幅降低 CPU 占用

6. **中文字库以占位符编译**：原始 FONT.H 中的 GBK 编码字符与 ARMCC V5 不兼容，字库索引替换为 `"__"` 占位（中文测试页未使用实际中文字符）

7. **FreeRTOS 队列 vs 环形队列**：TX/RX 使用 FreeRTOS 队列（线程安全、挂起等待），`components/my_queue.c` 为备用环形队列（当前未被 App 使用）

8. **CAN TX 邮箱保护**：`ModCommCan_Tx()` 邮箱满时用 `xQueueSendToFront` 回灌队首后 break（不丢数据，等待下次出队）

9. **ModCommCan_OnRxFrame 强符号**：原弱符号 `__weak` 已替换为强符号实现，直接解析心跳帧(0x320)和电机状态帧(0x110)写入 `g_dash_state`，不再需要应用层额外覆盖

10. **CAN 滤波器全通**：掩码=0，接收总线所有 29-bit 扩展帧，在 `ModCommCan_OnRxFrame()` 中按 `src==CAN_ADDR_MOTORBOARD` 过滤

11. **lv_conf.h include 链**：通过 `LV_CONF_INCLUDE_SIMPLE` 宏使 `lv_conf_internal.h` 走 `#include "lv_conf.h"` 路径，而非相对路径 `../../lv_conf.h`。同理 `LV_LVGL_H_INCLUDE_SIMPLE` 控制 LVGL 头文件方式

12. **仪表盘线程安全**：CAN RX (prio 4) 写入 g_dash_state，LCD_DEMO (prio 3) 读取，通过 FreeRTOS Mutex 保护。UI 更新用 `Dashboard_Data_GetSnapshot()` 获取本地快照后脱离锁操作

---

## 当前编译状态

- **App 工程**：`mdk/app.uvprojx`，双 Target（stm32f429 / stm32f429_b），armcc V5.06, C99
- **Bootloader 工程**：`mdk/boot.uvprojx`，独立编译
- **LVGL 源文件**：118 个 `.c` 文件通过 12 个 Keil 分组管理（新增加 lv_font_montserrat_28.c）
- **本工程新增源文件**：`dashboard_images.c`、`mod_dashboard_data.c`、`mod_dashboard_fault.c`、`task_dashboard_ui.c`、`mod_comm_uart.c`、`usart6.c`、`task_vofa.c`
- **注意（既有问题）**：`app_b.uvprojx` 未配置 LVGL include path（`..\third_lib\LVGL\lvgl` 等），编译 `task_dashboard_ui.h` 报 `lvgl.h` 找不到。本次新增文件（`usart6.c`/`task_vofa.c`）在 app_b 编译通过，该错误与本次改动无关；如需 A/B 双槽完整编译需补 LVGL 路径（另 app_b 还缺 `mod_dashboard_data`/`task_dashboard_ui` 等 dashboard 文件条目，属既有未完成状态）
- **Keil 编译器 Define**：`STM32F429_439xx,USE_STDPERIPH_DRIVER,LV_LVGL_H_INCLUDE_SIMPLE,LV_CONF_INCLUDE_SIMPLE`（B 槽额外 `APP_SLOT_B`）
- **Include Paths**：新增 `..\pic` 路径（包含 `..\bootloader;..\pic;..\third_lib\LVGL\lvgl;..\third_lib\LVGL\lvgl\examples\porting` 等）
- App A 槽 → 0x08020000，App B 槽 → 0x08080000

---

---

## 本次已完成 — UART 通信框架（查询链路拆分，2026-08-05）

**目标**：将原 `UART_Query_Task`（轮询解析 + 透传 CAN）拆分为 `UART_TX`/`UART_RX` 两个任务，框架参考 CAN（队列 + 任务 + 弱符号回调），bsp/mod/task 三层解耦。

**验证结果**：
- ✅ 环路验证脚本 `tools/uart_loopback_test.py` 实测 **2 PASS / 0 FAIL**
  - 测试1：`AA 55 01 00` → 应答 `AA 55 01 0D` + 13B（mcu=STM32F429, 分区=App A, boot=0x08000000, app=0x08020000, v1.0）
  - 测试2：`AA 55 10 04 01 02 03 04 <crc>` → 原样回发 echo
- ✅ 编译：app.uvprojx（stm32f429 A 槽）0 错 0 警；boot.uvprojx 0 错 0 警；app_b 中本次改动文件全部编译通过
- ✅ 调试日志确认全链路：ISR 收字节(isr 计数) → RX 任务 → 拼包 → 回调 → TX 发送

**关键设计**：
- ISR 直入 FreeRTOS 字节队列（替换原 64B 环形缓冲），对齐 CAN 架构
- `len==0` 帧立即回调（兼容 PC 旧查询指令 `[AA 55 01 00]` 无 CRC）；`len>0` 严格 CRC16(poly 0x1021) 校验
- UART_TX 用临界区发送，与 printf(fputc) 对 USART1 互斥
- 查询业务通过强符号覆盖 `ModCommUart_OnRxPacket`，不直接碰串口

**已清理**：
- ✅ 验证通过后已移除全部调试日志（逐字节 `LOG_D`、100ms 轮询诊断分支、回调入口 `LOG_I`），保留 `uart_rx_isr_cnt` 诊断计数 + Init 返回值检查 + CRC mismatch 警告（均为合理防御），最终固件编译 0 错 0 警

---

## 本次已完成 — LVGL 暂停按钮 + 隐藏刻度数字（2026-08-06）

**目标**：① 注释掉滑动条上方 0/50/100 三个刻度数字；② 在滑动条和仪表盘之间新增红色圆角矩形的"一键暂停"按钮。

**① 隐藏 0/50/100 刻度数字**
- `task_dashboard_ui.c` 中刻度标签容器 `tick_cont`（含 0/50/100 三个 `lv_label`）整块注释，保留代码便于恢复

**② PAUSE 一键暂停按钮（红色圆角矩形）**
- 位置：仪表盘与 Load Bar 之间，`y=200` 居中，80×30，`radius=8`，红色 `COLOR_ERROR_RED`，白色 "PAUSE" 文字（项目无中文字体，用英文）
- 交互（用户确认：**切换·恢复原值**）：
  - 按下进入暂停：保存当前滑块位置 `s_pre_pause_pct` → `rpm_target` 归 0 → 滑块 `lv_slider_set_value(0)` 并 `clear_flag(CLICKABLE)` 锁定 → 发 CAN 目标转速 0 帧 → 表盘/VOFA/滑块标签同步为 0
  - 再按解除暂停：滑块回到暂停前位置 → `rpm_target` 恢复原值（`pct×3`）→ 恢复 `CLICKABLE` → 发 CAN 原值帧
- `DashboardState` 新增 `paused` 标志；`on_load_change` 增加防御检查（暂停中忽略滑块变化）
- 抽取 `send_rpm_target()` 公共函数，`on_load_change` / `on_pause_click` 共用 CAN 下发

**验证**：app.uvprojx（stm32f429 A 槽）编译 **0 Error 0 Warning**

---

## 本次已完成 — LVGL 仪表盘改进 + VOFA 调试（2026-08-05）

**目标**：① 修复 CAN 指示灯不闪烁；② RPM 表盘值改由拖动条控制；③ 每 100ms 经串口发 firewater 格式 RPM 供 VOFA+ 看波形。

**① CAN 指示灯（最终方案：移除 Top Bar 图片）**
- 根因：`img_top_bar` 图片内嵌了静态状态点，无论遮挡还是对齐都不可靠，用户看到的恒是图片自带的静态点（常亮、不闪烁）
- 方案：**不再创建 Top Bar 背景图**。CAN 指示灯改用 LVGL 原生控件——6×6 圆形 `lv_obj`（在线绿色 500ms 闪烁 / 离线红色常亮，直接改 `bg_color`/`bg_opa`）+ `lv_label` "CAN" 文本框，完全不依赖任何图片像素
- 结果：**始终闪烁**（在线绿 / 离线红，500ms 周期，`now % 500 < 250`），颜色区分通信状态；彻底摆脱图片内容干扰。用户确认采用"始终闪烁，颜色区分"语义

**② RPM 由拖动条控制**
- 表盘数值改显示 `g_dash_state.rpm_target`（拖动条 0–100 ×3 → 0–300 RPM），不再显示 CAN 实测 `rpm`
- 拖动仍通过 `on_load_change` 保留 CAN 目标转速下发（`CanProto_SendFrame`）

**③ VOFA+ firewater 波形输出（临时）**
- 新增 `driver/usart6.c/h`：USART6 (PC6/PC7, AF8, APB2, 115200)，仅发送
- 新增 `task/task_vofa.c/h`：`Vofa_Task` 每 100ms 读 `rpm_target`，以 firewater ASCII 格式发 `"%u\r\n"`
- USART6 与 USART1（日志 + 查询协议）完全隔离，波形干净
- **临时**：`VOFA_DEBUG` 开关（task_vofa.h），测试完成后置 0 或整段删除（usart6.c/h、task_vofa.c/h、task_entry 引用、mdk 工程引用）

**验证**：app.uvprojx（stm32f429 A 槽）编译 **0 Error 0 Warning**；app_b 仅报既有 `lvgl.h` 缺失问题，未引入新错误

---

## 下一步待做工作（按优先级）

### P1 — 编译验证与调试

- [x] Keil 编译 0 error 0 warning 验证（`stm32f429` A 槽）
- [x] 移除 Top Bar 图片，CAN 指示灯改用 LVGL 原生圆点 + 文本框
- [ ] 烧录测试：LCD 显示仪表盘全貌，确认 CAN 指示灯**红色闪烁**（无心跳，始终闪）
- [ ] 接动力域心跳后验证 CAN 指示灯变**绿色闪烁**（颜色切换正常）
- [ ] 验证 Load Bar 交互 → CAN TX 帧发送 + 表盘 RPM 实时变化
- [ ] VOFA+ 接 USART6 (PC6/PC7) 验证 RPM 波形（100ms 间隔，firewater ASCII）
- [ ] 烧录测试：PAUSE 按钮 → 表盘/VOFA/CAN 帧归 0、滑块锁定；再按恢复暂停前值

### P2 — 动力域 CAN 协议帧补齐

当前 `MOD_ID_STATUS_MOTOR(0x110)` 帧格式为显示域预设，需要对齐动力域实现：
- 确认动力域上报的电机状态帧格式（rpm、odo、status 字段）
- 心跳帧格式已定义，待动力域开始发送后联调

### P3 — 电机传感器真实数据接入

将 `driver/mod_motor.c` 的占位函数替换为实际实现：
- 当前 `Mod_Motor_Get_Speed()` 和 `Mod_Motor_Angle()` 均硬编码返回 0.0f
- 需要对接真实传感器数据源（通过 CAN 从动力域获取后写入）

### P4 — CAN 滤波器细化（`app/bsp_can.c`）

当前全通（掩码=0），需要按源地址过滤：
- 只接收来自 `CAN_ADDR_MOTORBOARD` (0x02) 的帧
- 减少中断负载

### P5 — 栈深度监控

LVGL 嵌套渲染可能较深，当前 LCD_DEMO 栈为 1024 字 (4KB)：
- 如果出现栈溢出，扩展到 2048 字 (8KB)
- 可用 FreeRTOS `uxTaskGetStackHighWaterMark()` 监控

---

## 已知注意事项

1. **CCM 与 DMA**：FreeRTOS 堆在 CCM（`0x10000000`），DMA 无法访问。任何 DMA 缓冲必须放在 `.bss`/全局数组（落在 `0x20000000` 主 SRAM）

2. **`#include "can_protocol.h"` 大小写**：`mod_comm_can.h:8` include `"can_protocol.h"`，在 Windows（大小写不敏感）下能匹配 `protocol/CAN_Protocol.h`。如果迁移到 Linux CI 构建须改为 `"CAN_Protocol.h"`

3. **Delay_us/ms 与 FreeRTOS**：`driver/Delay.c` 直接操作 SysTick 寄存器，与 FreeRTOS 的 SysTick 中断冲突。App 内应使用 `vTaskDelay()`，不可调用 `Delay_us()/Delay_ms()`

4. **SPI 速度**：ILI9341 初始化后 LCD_Clear 用直接寄存器访问方式，`SPI_WriteByte()` 用于普通操作，读操作时降速至预分频 16

5. **触摸 ID 验证**：`FT6336_Init()` 需验证 VENDOR(0xA8=0x11)、MID(0x9F=0x26)、HIGH(0xA3=0x64)，任一失败返回 1

6. **AB 分区调试**：编译 B 槽 Target 时，如果要将 B 镜像烧入 A 槽地址调试，需临时切换 scatter 起始地址

7. **仪表盘图片数据在 Flash**：`dashboard_images.c` 的 const 数据位于 .rodata/Flash；LVGL 绘制缓冲区和 DMA 缓冲均使用主 SRAM，FreeRTOS CCM 堆指针不用于 DMA

8. **mode.bin 图片**：已转换为 24×12 像素数据，但在 `Dashboard_UI_Init()` 中未放置到 UI（计划中的档位 "D" 指示器，Top Bar 已固化此内容）
