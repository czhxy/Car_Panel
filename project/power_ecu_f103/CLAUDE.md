# CLAUDE.md — 动力域 ECU（STM32F103C8T6）

> 总体概览。当前进度和待完成任务见 `HANDOFF.md`。

## 角色

本 ECU 是双 MCU 系统中的**动力域控制器**：

| 项目 | 值 |
|---|---|
| MCU | STM32F103C8T6 (Cortex-M3) |
| 主频 | 72MHz（HSE 8MHz → PLL ×9） |
| Flash | 64KB |
| RAM | 20KB |
| RTOS | 无（裸机开发，主循环 + SysTick 中断驱动多周期调度） |
| Bootloader | 无（单 App，从 0x08000000 直接启动） |

**对上通信**：通过 CAN 总线（500kbps, 29-bit 扩展帧）向显示域 ECU（STM32F429）上报状态、接收控制指令。

## 当前状态概要

CAN 通信基础设施已完整实现并修复（TX/RX 双队列、中断接收、SCE 错误恢复、多周期调度框架）。电机驱动、CAN 协议帧组装、串口日志、故障保护尚未实现。

**详细进度 → [`HANDOFF.md`](./HANDOFF.md)**

## 工程文件分组（Keil 内）

| 组名 | 主要文件 | 说明 |
|---|---|---|
| Start | startup_stm32f10x_md.s, core_cm3.c, system_stm32f10x.c | 启动文件 + CMSIS |
| Library | SPL 全部外设库 | ADC/CAN/DMA/GPIO/TIM/USART 等 |
| System | Delay.c/h, sysclock.c/h | 精确延时 + 周期调度 |
| Mod | Mod_Comm_Can.c/h, Mod_Motor.c/h, Mod_Usart.c/h | 模块层（业务封装） |
| component | queue.c/h | 环形队列 |
| protocol | CAN_Protocol.h | CAN ID 位域定义 |
| driver | drv_can.c/h, drv_motor.c/h, drv_usart.c/h | 底层驱动 |
| task | task_comm_can.c/h, task_motor_ctl.c/h, task_uart.c/h | 任务调度 |
| User | main.c/h, stm32f10x_conf.h, stm32f10x_it.c/h | 用户代码 |

## 硬件引脚分配

### 左电机

| 功能 | 引脚 | 外设 | 实现状态 |
|---|---|---|---|
| PWM (H 桥) | PA8 | TIM1_CH1 | 已实现 |
| DIR (方向) | PA4 | GPIO | 已实现 |
| EN (使能) | PB0 | GPIO | 已实现 |
| Encoder A | PA0 | TIM2_CH1 | 已实现 |
| Encoder B | PA1 | TIM2_CH2 | 已实现 |

### 右电机

| 功能 | 引脚 | 外设 | 实现状态 |
|---|---|---|---|
| PWM (H 桥) | PB8 | TIM4_CH3 | 已实现 |
| DIR (方向) | PB9 | GPIO | 已实现 |
| EN (使能) | PA5 | GPIO | 已实现 |
| Encoder A | PA6 | TIM3_CH1 | 已实现 |
| Encoder B | PA7 | TIM3_CH2 | 已实现 |

### 其他外设

| 功能 | 引脚 | 外设 | 实现状态 |
|---|---|---|---|
| CAN1_TX | PA12 | CAN1 | 已实现 |
| CAN1_RX | PA11 | CAN1 | 已实现 |
| LED_RUN | PC13 | GPIO | 待实现 |
| LED_FAULT | PB1 | GPIO | 待实现 |
| KEY_LOCAL | PB2 | GPIO | 待实现 |
| USART1_TX | PA9 | USART1 | 已实现 |
| USART1_RX | PA10 | USART1 | 已实现 |

## CAN 协议（须与显示域对齐）

CAN ID 位域定义参见 `./protocol/CAN_Protocol.h`：
- 29 位扩展帧：`[28:26]prio [25:22]src [21:18]dst [17:16]ftype [15:6]mode [5:0]func`
- 设备地址：`CAN_ADDR_MAINBOARD=0x01`, `CAN_ADDR_MOTORBOARD=0x02`
- `CAN_SELF_ADDR` 已正确设为 `CAN_ADDR_MOTORBOARD`（不要再改回 MAINBOARD）

**本 ECU 需实现的帧**：

| 方向 | Mode ID | 周期 | 说明 |
|---|---|---|---|
| 发送 | 0x110 (STATUS_MOTOR) func=0x00 | 20ms | 左电机状态：转速、电流、编码器角度 |
| 发送 | 0x110 (STATUS_MOTOR) func=0x01 | 20ms | 右电机状态：转速、电流、编码器角度 |
| 发送 | 0x101 (ALERT) / 自定 | 按需 | 故障诊断信息 |
| 发送 | 0x320 (HEARTBEAT) | 500ms | 心跳帧 |
| 接收 | 0x020 (CTRL_LF) | 50ms | 左电机控制指令 |
| 接收 | 0x021 (CTRL_RF) | 50ms | 右电机控制指令 |

## 现有可用组件

### 1. 环形队列 (`component/queue/`)

```c
QueueType q;
uint8_t pool[32 * sizeof(msg)];
Queue_Init(&q, pool, sizeof(pool), sizeof(msg));
Queue_Put(&q, &msg);    // 入队（满则 false）
Queue_Get(&q, &msg);    // 出队（空则 false）
Queue_Query(&q, &msg);  // 只读队首
```

### 2. 延时函数 (`System/Delay.c`)

```c
Delay_us(x);   // 微秒延时（0~233015 μs）
Delay_ms(x);   // 毫秒延时
Delay_s(x);    // 秒延时
```

**注意**：会直接操作 SysTick 寄存器，而 sysclock.c 使用 SysTick 中断作时基，两者不可同时使用。当前主循环使用 `tpf` 周期标志调度，不需要 Delay。

### 3. 周期调度 (`System/sysclock.c`)

基于 SysTick 中断，提供 8 个周期的标志位：

| 周期 | 标志位 | 主循环中使用 |
|---|---|---|
| 1ms | `tpf.task_period_1ms` | — |
| 5ms | `tpf.task_period_5ms` | `Task_Motor_Ctl()` |
| 10ms | `tpf.task_period_10ms` | CAN TX/RX |
| 20ms | `tpf.task_period_20ms` | UART TX/RX |
| 100ms | `tpf.task_period_100ms` | — |
| 200ms | `tpf.task_period_200ms` | — |
| 500ms | `tpf.task_period_500ms` | —（心跳帧） |
| 1000ms | `tpf.task_period_1000ms` | — |

### 4. CAN 中断与处理函数

| 中断源 | 向量名 | 处理函数 | 文件 |
|---|---|---|---|
| CAN RX0 (FIFO0) | `USB_LP_CAN1_RX0_IRQHandler` | `USB_LP_CAN1_RX0_IRQHandler` | `driver/drv_can.c` |
| CAN SCE | `CAN1_SCE_IRQHandler` | `CAN1_SCE_IRQHandler` | `driver/drv_can.c` |
| SysTick | `SysTick_Handler` | `SysTick_Handler` → `SysClock_Cb()` | `System/sysclock.c` |

## 与显示域 ECU 的依赖关系

- 两个 ECU 通过 CAN 总线耦合，**CAN 协议 ID 位域定义必须一致**
- `protocol/CAN_Protocol.h` 是显示域 `CAN_Protocol.h` 的本地拷贝（纯 C，无 F4 依赖）
- `CAN_SELF_ADDR = CAN_ADDR_MOTORBOARD` 已经正确设置

## 资源约束

- **Flash 仅 64KB**（`0x08000000` 起始），含 SPL 库后剩余约 40KB
- **RAM 仅 20KB**（`0x20000000` 起始），需注意堆栈分配
- 编译器优化等级 1，Flash 不够时可调至 2 或 3
- SPL 的 USE_FULL_ASSERT 已禁用（节省 Flash）

## 编码约定

- 初始化在 `main.c` 中完成
- 主循环轮询 + SysTick 中断驱动，不用阻塞延时
- CAN 接收用中断 FIFO + 环形队列缓冲
- 故障处理统一用状态机
- 驱动层前缀 `drv_`，模块层前缀 `Mod_`，任务层前缀 `Task_`
- 注释使用中文，UTF-8 编码
