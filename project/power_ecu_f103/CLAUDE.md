# CLAUDE.md — 动力域 ECU（STM32F103C8T6）

> 仅包含动力域相关内容。全项目上下文还会自动加载根目录 `../CLAUDE.md`。

## 角色

本 ECU 是双 MCU 系统中的**动力域控制器**：

| 项目 | 值 |
|---|---|
| MCU | STM32F103C8T6 (Cortex-M3) |
| 主频 | 72MHz（HSE 8MHz → PLL ×9） |
| Flash | 64KB |
| RAM | 20KB |
| RTOS | 无（裸机开发） |
| Bootloader | 无（单 App，从 0x08000000 直接启动） |

**对上通信**：通过 CAN 总线（500kbps, 29-bit 扩展帧）向显示域 ECU（STM32F429）上报状态、接收控制指令。

## 当前完成状态

| 模块 | 文件 | 状态 |
|---|---|---|
| Keil 工程 | `Project.uvprojx` | 已配置（armcc V5.06, C99, 优化等级 1） |
| 标准外设库 | `Library/` | 已集成 SPL V3.5.0 |
| 启动 + 时钟 | `Start/` | 已配置 72MHz |
| 环形队列 | `component/queue/queue.c` | 已实现 |
| 延时函数 | `System/Delay.c` | 已实现（SysTick 轮询，裸机用） |
| 中断模板 | `User/stm32f10x_it.c` | 默认模板（故障死循环） |
| **drv_can** | `driver/drv_can.c` | **空壳（只有 #include）** |
| **drv_usart** | `driver/drv_usart.c` | **空壳（只有 #include）** |
| **task_comm_can** | `task/task_comm_can.c` | **空文件** |
| **task_motor_ctl** | `task/task_motor_ctl.c` | **空文件** |
| **task_uart** | `task/task_uart.c` | **空文件** |
| **main.c** | `User/main.c` | **空死循环** |

## 工程文件分组（Keil 内）

| 组名 | 文件 | 说明 |
|---|---|---|
| Start | startup_stm32f10x_md.s, core_cm3.c, system_stm32f10x.c | 启动文件 + CMSIS |
| Library | SPL 全部外设库 | ADC/CAN/DMA/GPIO/TIM/USART 等 |
| System | Delay.c/h | 精确延时 |
| Hardware | (空) | 待添加板级外设 |
| driver | drv_can.c/h, drv_usart.c/h | 底层驱动 |
| task | task_comm_can.c/h, task_motor_ctl.c/h, task_uart.c/h | 业务任务 |
| User | main.c/h, stm32f10x_conf.h, stm32f10x_it.c/h | 用户代码 |

## 硬件引脚分配（尚未编码实现）

| 功能 | 引脚 | 外设 |
|---|---|---|
| CAN1_TX | PA12 | CAN1 |
| CAN1_RX | PA11 | CAN1 |
| PWM (电机 H 桥) | PA8 | TIM1_CH1 |
| DIR (方向) | PA4 | GPIO |
| EN (使能) | PB0 | GPIO |
| Encoder A | PA0 | TIM2_CH1 |
| Encoder B | PA1 | TIM2_CH2 |
| LED_RUN | PC13 | GPIO |
| LED_FAULT | PB1 | GPIO |
| KEY_LOCAL | PB2 | GPIO |
| USART1_TX | PA9 | USART1 |
| USART1_RX | PA10 | USART1 |

## CAN 协议（须与显示域对齐）

CAN ID 编码规范参见 `../display_ecu_f429/protocol/CAN_Protocol.h`，该文件定义了：
- 29 位扩展帧 ID 位域划分（优先级/源地址/目标地址/帧类型/mode_id/功能字段）
- `CAN_ID_BUILD` / `CAN_ID_GET_*` 宏、`CanProto_EncodeId/DecodeId` 编解码函数
- 设备地址：`CAN_ADDR_MAINBOARD=0x01`, `CAN_ADDR_MOTORBOARD=0x02`
- Mode ID 表：电机控制(0x020)、电机状态(0x110)、心跳(0x320) 等

**本 ECU 需实现的帧**：

| 方向 | Mode ID | 周期 | 说明 |
|---|---|---|---|
| 发送 | 0x110 (STATUS_MOTOR) | 20ms | 当前转速、电流、编码器角度 |
| 发送 | 0x101 (ALERT) / 自定 | 按需 | 故障诊断信息（堵转、丢编码器、CAN 超时） |
| 发送 | 0x320 (HEARTBEAT) | 500ms | 心跳帧 |
| 接收 | 0x020 (CTRL_LF) 等 | 50ms | 来自显示域的电机控制指令（目标转速/电流） |

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

纯 C 实现，无 RTOS 依赖，`front == rear` 区分空/满。

### 2. 延时函数 (`System/Delay.c`)

```c
Delay_us(x);   // 微秒延时（0~233015 μs）
Delay_ms(x);   // 毫秒延时
Delay_s(x);    // 秒延时
```

基于 SysTick 硬件计数器轮询（非中断）。注意：会覆盖 SysTick 寄存器，如果后续需要 SysTick 中断作时基，需改用别的定时器。

## 开发优先级

1. **`driver/drv_can.c`** — CAN 外设初始化、滤波器配置（500kbps, 29-bit ext）、收发函数。这是所有通信的基础
2. **`driver/drv_usart.c`** — 串口日志输出（调试必备）
3. **`User/main.c`** — 主循环框架：初始化外设 → 进入循环调度
4. **`task/task_motor_ctl.c`** — PWM 输出（TIM1_CH1, PA8）+ 编码器读取（TIM2_CH1/CH2, Encoder Mode）+ PID 闭环
5. **`task/task_comm_can.c`** — CAN 协议打包/解析，对接显示域的 `CAN_Protocol.h` 定义，组装 0x110/0x320 帧，解析 0x020 帧
6. **故障保护** — IWDG（1s）、堵转检测（500ms 无编码器变化）、编码器丢失检测（100ms）、CAN 超时自动停机（200ms 未收到指令）

## 与显示域 ECU 的依赖关系

- 两个 ECU 通过 CAN 总线耦合，**CAN 协议 ID 位域定义必须一致**
- 建议本工程引用或拷贝 `../display_ecu_f429/protocol/CAN_Protocol.h` 中的宏和类型定义（纯 C 的 `.h`，无 F4 依赖，可直接复用）
- `CAN_Protocol.h` 中 `CAN_SELF_ADDR` 当前定义为 `CAN_ADDR_MAINBOARD`，本 ECU 需改为 `CAN_ADDR_MOTORBOARD`

## 资源约束

- **Flash 仅 64KB**（`0x08000000` 起始），含 SPL 库后剩余约 40KB 用于业务代码
- **RAM 仅 20KB**（`0x20000000` 起始），需注意堆栈分配
- 编译器优化等级当前为 1，如果 Flash 不够可调至 2 或 3
- SPL 的 USE_FULL_ASSERT 当前已禁用（节省 Flash）

## 编码约定

- 初始化在 `main.c` 中完成（非 RTOS 环境，不用任务）
- 主循环轮询或定时器中断驱动，不用 RTOS 延时阻塞
- CAN 接收用中断 FIFO + 环形队列缓冲，收到后消费
- 故障处理统一用状态机，不随地散落 `while(1);`\n- 驱动层前缀 `BSP_`（如 `BSP_CAN_Init`），任务层前缀 `Task_` 或 `Mod_`
