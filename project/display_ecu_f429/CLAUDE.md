# CLAUDE.md — 显示域 ECU（STM32F429IGT6）

> 总体概览。全项目上下文还会自动加载根目录 `../../CLAUDE.md`。详细进度见 `HANDOFF.md`。

## 角色

本 ECU 是双 MCU 系统中的**显示域控制器**：

| 项目 | 值 |
|---|---|
| MCU | STM32F429IGT6 (Cortex-M4, FPU) |
| 主频 | 180MHz（HSE 8MHz → PLL ×180） |
| Flash | 2MB（项目使用前 1MB） |
| RAM | 128KB 主 SRAM + 64KB CCM |
| RTOS | FreeRTOS v11.3.0（heap_4, 64KB CCM 堆） |
| Bootloader | 有（YMODEM OTA，真 AB 分区） |

**对下通信**：通过 CAN 总线（500kbps, 29-bit 扩展帧）向动力域 ECU（STM32F103）发送控制指令、接收状态上报。

## 当前状态概要

CAN 通信框架完整运行（TX/RX 双队列、中断接收、心跳、ID 编解码、电机控制帧周期发送）。关键短板：LTDC LCD 驱动和 LVGL 仪表盘 UI 完全未开发。

**详细进度 → [`HANDOFF.md`](./HANDOFF.md)**

## 工程文件分组

```
display_ecu_f429/
  app/                  BSP 驱动层（bsp_can/led/key/log）
  bootloader/           Bootloader（YMODEM + OTA 决策）
  components/           可复用组件（环形队列）
  driver/               底层驱动（USART、延时、电机传感器占位）
  firmware/             CMSIS + STM32F4xx SPL
  task/                 FreeRTOS 任务
  third_lib/            FreeRTOS v11.3.0
  mdk/                  Keil 工程 + scatter 文件
  protocol/             CAN 协议定义 + Python 工具
  tools/                YMODEM 发送工具
  docs/                 设计文档 + 变更记录
```

## FreeRTOS 配置

| 配置项 | 值 |
|---|---|
| 版本 | V11.3.0 |
| Tick 频率 | 1000Hz |
| 堆大小 | 64KB（heap_4，位于 CCM `0x10000000`） |
| 最大优先级 | 32 |
| 最小栈 | 128 字 |
| FPU | 启用 |

**已创建的任务**：

| 任务 | 栈 | 优先级 | 函数 |
|---|---|---|---|
| ALL_Task_Entry | 256 字 | 30 | 一次性初始化入口，创建以下 6 个任务 |
| CAN_TX | 512 字 | 3 | `Mod_Can_TxTask` |
| CAN_RX | 512 字 | 3 | `Mod_Can_RxTask` |
| CAN_TEST | 256 字 | 3 | `CAN_Test_Task` |
| KEY_SCAN | 256 字 | 2 | `prvKeyScanTask` |
| HEARTBEAT | 512 字 | 1 | `Heartbeat_Task` |
| UART_QUERY | 256 字 | 2 | `UART_Query_Task` |

## CAN 通信架构

### 协议定义

参见 `protocol/CAN_Protocol.h`：
- 29 位扩展帧：`[28:26]prio [25:22]src [21:18]dst [17:16]ftype [15:6]mode [5:0]func`
- `CAN_SELF_ADDR = CAN_ADDR_MAINBOARD (0x01)` → 向 `CAN_ADDR_MOTORBOARD (0x02)` 发送控制帧
- CAN 波特率：500kbps（Prescaler=9, BS1=7tq, BS2=2tq @ 180MHz SYSCLK/45MHz APB1）

### CAN ID 编码函数

```c
// protocol/CAN_Protocol.h 提供
CanProto_EncodeId(src, dst, prio, ftype, mode, func) → uint32_t id
CanProto_DecodeId(id, *proto) → 解析到 CanProtocolId 结构体
CAN_ID_BUILD(prio, src, dst, ftype, mode, func) → 宏版本
```

### 数据流

```
TX: 应用层 → ModCanFrame → Mod_Can_TxEvent() → CanTxQueue (FreeRTOS队列, 深度64)
       → ModCommCan_Tx() → CanTxMsg → CAN_Transmit() → 硬件邮箱
       邮箱满 → xQueueSendToFront 回灌队首，break

RX: CAN FIFO0 ISR → Mod_Can_RxIRQHandler() → CanRxQueue (FreeRTOS队列, 深度64)
       → Mod_Can_RxTask() → ModCommCan_OnRxFrame() (弱符号，可被应用层强符号覆盖)
```

### 当前发送的帧

| 帧 | 函数 | 周期 | 内容 |
|---|---|---|---|
| 心跳 | `Can_Heartbeat()` | 500ms | 4 字节计数 + 4 字节状态 |
| 电机控制 | `CanProtocol_WheelCtlSend()` | 10ms 限频 | 转速/角度（当前 Mod_Motor 返回 0） |
| 测试帧 | `Mod_Can_TxTest()` | 按键触发 | 8 字节递增测试数据 |

## 引脚分配

| 功能 | 引脚 | 说明 |
|---|---|---|
| CAN1_TX | PA12 | 已实现 |
| CAN1_RX | PA11 | 已实现 |
| USART1_TX | PA9 | printf 日志输出 |
| USART1_RX | PA10 | 查询协议命令接收 |
| LED1 | PH12 | 心跳指示 |
| LED2 | PH10 | — |
| LED3 | PH11 | — |
| LED4 | PE3 | — |
| KEY1 | PE2 | CAN 测试发送 |
| KEY2 | PI11 | 预留 |

## 编码约定

- BSP 层：`app/bsp_<module>.c/h` — 硬件抽象
- 任务层：`task/mod_<module>.c/h`（模块）或 `task/task_<module>.c/h`（任务）
- 日志：`LOG_E/W/I/D` → `printf()` + `\r\n`
- `ModCommCan_OnRxFrame()` 是弱符号（`__weak`），应用层可定义同名强符号覆盖
- 注释和文档用中文
