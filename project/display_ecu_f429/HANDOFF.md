# HANDOFF.md — 显示域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 模块完成度总表

| 模块 | 完成度 | 文件/位置 | 说明 |
|---|---|---|---|
| Bootloader (YMODEM OTA) | 100% | `bootloader/` | YMODEM-1K + 真 AB 分区 + CRC32 校验 |
| OTA 参数管理 | 100% | `bootloader/ota_params.c` | Sector 4，append-only 日志，1024 槽 × 64B，掉电安全 |
| FreeRTOS 集成 | 100% | `third_lib/FreeRTOS` | v11.3.0，heap_4 @ CCM，6 任务运行 |
| CAN 通信框架 | 100% | `task/mod_comm_can.c/h` | TX/RX 双 FreeRTOS 队列，ISR 接收，心跳，测试帧 |
| CAN 协议层 | 90% | `task/task_comm_can_protocol.c/h`, `protocol/CAN_Protocol.h` | ID 编解码完成，电机控制帧周期发送，调试查询占位 |
| 串口日志 | 100% | `app/bsp_log.h`, `driver/usart.c/h` | printf 重定向，`LOG_E/W/I/D` 宏，环形缓冲 RX |
| 按键驱动 | 100% | `app/bsp_key.c/h` | 20ms 扫描，上升沿检测，FreeRTOS 信号量通知 |
| LED 驱动 | 100% | `app/bsp_led.c/h` | 4 路 GPIO 输出 |
| UART 查询服务 | 100% | `task/task_query.c/h` | `0xAA 0x55` 协议，芯片信息/VTOR 自证槽位查询 |
| 电机传感器驱动 | 5% | `driver/mod_motor.c/h` | 仅占位函数（返回 0.0f） |
| SPI LCD 驱动 (ILI9341V) | 0% | `app/bsp_spi_lcd.c/h`（待建） | SPI2 4 线接口，240×320 RGB565 |
| I2C 触摸驱动 (FT6336G) | 0% | `app/bsp_i2c_touch.c/h`（待建） | I2C1 400kHz，2 点触摸 |
| LVGL 仪表盘 UI | 0% | `task/task_display.c`（待建） | 未创建 |

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
│  Mod_Can_TxTask (prio 3, 1ms):                             │
│    → ModCommCan_Tx() 出 TX 队列 → CAN_Transmit()            │
│    → CanProtocol_WheelCtlSend() → 10ms 限频电机控制帧        │
│    → CanProtocol_WheelDebugQuery() (空壳)                    │
│                                                            │
│  Mod_Can_RxTask (prio 3):                                  │
│    → xQueueReceive(RxQueue) → ModCommCan_OnRxFrame()        │
│                                                            │
│  Heartbeat_Task (prio 1, 500ms):                            │
│    → LED 翻转 + Can_Heartbeat() → Mod_Can_TxEvent()         │
│                                                            │
│  UART_Query_Task (prio 2):                                 │
│    → 状态机解析 0xAA 0x55 协议 → 查询应答                     │
│                                                            │
│  KEY_SCAN (prio 2, 20ms):                                   │
│    → 按键扫描 → xSemaphoreGive                              │
│                                                            │
│  CAN_Test_Task (prio 3):                                   │
│    → 等待 xKey1Sem → Mod_Can_TxTest()                       │
└────────────────────────────────────────────────────────────┘

当前问题：
- CanProtocol_WheelCtlSend() 读取 Mod_Motor_Get_Speed/Angle() 均返回 0.0f
- 所以发送出去的电机控制帧数据字段始终为 0
```

---

## 关键设计决策

1. **FreeRTOS 队列 vs 环形队列**：TX/RX 使用 FreeRTOS 队列（线程安全、挂起等待），`components/my_queue.c` 为备用环形队列（当前未被 App 使用）

2. **CAN TX 邮箱保护**：`ModCommCan_Tx()` 邮箱满时用 `xQueueSendToFront` 回灌队首后 break（不丢数据，等待下次出队）

3. **弱符号回调**：`ModCommCan_OnRxFrame()` 为 `__weak` 弱符号，应用层可定义同名强符号覆盖默认行为

4. **VTOR 自证槽位**：`task_query.c` 用 `(SCB->VTOR == 0x08020000)?1:2` 判断运行槽，不读 OTA 参数区（避免耦合）

5. **CAM 滤波器全通**：掩码=0，接收总线所有 29-bit 扩展帧，后续可按源地址滤波器细化

6. **CAN_SELF_ADDR = CAN_ADDR_MAINBOARD**：本 ECU 是主板，`CAN_Protocol.h` 中已定义正确

---

## 当前编译状态

- **App 工程**：`mdk/app.uvprojx`，双 Target（stm32f429 / stm32f429_b），armcc V5.06, C99
- **Bootloader 工程**：`mdk/boot.uvprojx`，独立编译
- **已知警告**：`port.c` (FreeRTOS 移植层) 有编译警告，不影响功能
- app A 槽 → 0x08020000，app B 槽 → 0x08080000

---

## 下一步待做工作（按优先级）

### P0 — SPI LCD + I2C 触摸驱动

点亮屏幕是所有后续 UI 工作的前提。方案已从 LTDC 改为 SPI 2.8 寸触摸屏（MSP2834）：

- **LCD**：ILI9341V，SPI2（PB13=SCK / PB15=MOSI / PB14=MISO / PB12=CS / PB10=DC / PB11=RST），240×320 RGB565
- **触摸**：FT6336G，I2C1（PB6=SCL / PB7=SDA / PB8=INT / PB9=RST），地址 0x38
- **背光**：PB0 高电平点亮（模块 N-MOSFET 驱动，高=ON）
- SPI 模式 0（CPHA=0, CPOL=0），MSB 优先
- 需创建 `app/bsp_spi_lcd.c/h` + `app/bsp_i2c_touch.c/h`
- 详细方案见 `docs/spi_touch_screen_plan.md`

### P1 — LVGL 仪表盘 UI（`task/task_display.c`）

依赖 P0 LCD/触摸驱动完成：

- LVGL v8.x 移植：双缓冲（2×240×30×2 = 28.8KB 主 SRAM）、SPI flush 回调、FT6336G indev 回调
- 仪表盘界面渲染（速度表盘、方向指示灯、电量/油量等）
- 创建 LVGL 渲染任务（单独 FreeRTOS 任务，优先级 2，5ms 周期）
- 数据驱动：将 CAN 接收到的电机状态映射到 LVGL 控件

### P2 — 电机传感器真实数据接入

将 `driver/mod_motor.c` 的占位函数替换为实际实现：

- 当前 `Mod_Motor_Get_Speed()` 和 `Mod_Motor_Angle()` 均硬编码返回 0.0f
- 需要对接真实传感器数据源（通过 CAN 从动力域获取后写入）

### P3 — 动力域 CAN 协议帧补齐

当前向动力域发送的电机控制帧数据字段为 0（因为传感器返回 0.0f），需要：
- 实现有意义的电机目标转速/电流设置
- 接收和解析动力域上报的电机状态帧（0x110）、心跳帧（0x320）
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

4. **CAN ID 测试帧**：`CAN_TX_ID` 使用 `CAN_ADDR_BROADCAST` 目标 + 优先级 ALERT + mode 0x001，仅用于开发测试

5. **port.c 编译警告**：FreeRTOS v11.3.0 移植层的 port.c 在 armcc5 下有已知警告（未使用变量），不影响功能

6. **AB 分区调试**：编译 B 槽 Target 时，如果要将 B 镜像烧入 A 槽地址调试，需临时切换 scatter 起始地址
