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

## 元件清单

| 元件 | 型号 | 数量 | 说明 |
|---|---|---|---|
| MCU | STM32F103C8T6 | 1 | 动力域主控 |
| 电机驱动 | TB6612FNG | 1 | 双 H 桥驱动器（A 通道左/B 通道右） |
| 编码电机 | MG310 | 2 | 直流减速电机（自带 AB 相编码器） |
| 稳压模块 | DC-DC 多路输出 | 1 | 7.4V + 5V + 3.3V 输出 |

## 供电架构

```
7.4V 电池 ──→ 稳压模块 IN
                │
                ├── 7.4V OUT ──→ TB6612FNG VM（电机驱动动力电）
                │
                ├── 5V   OUT ──┬→ TB6612FNG VCC（芯片逻辑电源）
                │              ├→ TB6612FNG STBY（硬拉高，始终使能）
                │              ├→ 左 MG310 编码器 VCC
                │              └→ 右 MG310 编码器 VCC
                │
                ├── 3.3V OUT ──┬→ STM32F103 VDD/VDDA/VBAT
                │              └→ (其他 3.3V 外设)
                │
                └── GND ──────── 所有模块共地（电池负极、稳压模块、
                                 STM32、TB6612、编码器）
```

> **关键约束**：所有模块 GND 必须共地。所有电压全部从稳压模块引出，不直接从电池取电。TB6612FNG 仅一片，A/B 通道分别驱动左/右电机。STBY 硬接 VCC(5V) 始终使能。

## 硬件连线

### 完整连线图（单 TB6612FNG，A 通道左 / B 通道右）

```
STM32F103                         TB6612FNG
┌──────────┐              ┌──────────────────────┐
│          │              │                      │
│ PA8 ─────┼─────────────→│ PWMA       BO1 ──────┼────────→ MG310(左) M+
│ PA4 ─────┼─────────────→│ AIN1       BO2 ──────┼────────→ MG310(左) M-
│          │  GND ───────→│ AIN2                  │
│          │              │                      │
│ PB8 ─────┼─────────────→│ PWMB       AO1 ──────┼────────→ MG310(右) M+
│ PB9 ─────┼─────────────→│ BIN1       AO2 ──────┼────────→ MG310(右) M-
│          │  GND ───────→│ BIN2                  │
│          │              │                      │
│          │  5V ────────→│ STBY        VM ──────┼── 7.4V（稳压模块）
│          │              │              VCC ─────┼── 5V（稳压模块）
│          │              │              GND ─────┼── GND（共地）
│          │              │                      │
│ PA0 ─────┼─────────────────────────────────────┼────────→ MG310(左) A (编码器)
│ PA1 ─────┼─────────────────────────────────────┼────────→ MG310(左) B (编码器)
│ PA6 ─────┼─────────────────────────────────────┼────────→ MG310(右) A (编码器)
│ PA7 ─────┼─────────────────────────────────────┼────────→ MG310(右) B (编码器)
│          │              │                      │
│ GND ─────┼───────── GND ─── 所有模块共地       │
│          │              │                      │
│              MG310 编码器供电：                 │
│              5V（稳压模块）──→ 左/右编码器 VCC   │
└──────────┘              └──────────────────────┘
```

> **关键**：TB6612FNG 仅一片。STBY 硬接 VCC(5V) 始终使能，AIN2/BIN2 硬接 GND。单电机启停通过 PWM=0（刹车）实现。PB0、PA5 已释放为可用引脚。

### MG310 电机线序（六线引出，编码器集成）

| 电机线 | 左电机 | 右电机 | 说明 |
|---|---|---|---|
| M+ | TB6612 AO1 (A通道) | TB6612 BO1 (B通道) | 电机驱动正极 |
| M- | TB6612 AO2 (A通道) | TB6612 BO2 (B通道) | 电机驱动负极 |
| VCC | 5V（稳压模块） | 5V（稳压模块） | 编码器供电 |
| GND | GND（共地） | GND（共地） | 编码器地 |
| A | STM32 PA0 (TIM2_CH1) | STM32 PA6 (TIM3_CH1) | 编码器 A 相 |
| B | STM32 PA1 (TIM2_CH2) | STM32 PA7 (TIM3_CH2) | 编码器 B 相 |

### TB6612FNG 控制引脚表（单芯片，A=左/B=右）

| TB6612 引脚 | 连接对象 | 功能 |
|---|---|---|
| PWMA | PA8 (TIM1_CH1) | 左电机 PWM |
| AIN1 | PA4 (GPIO 推挽) | 左电机方向 |
| AIN2 | GND | 低侧固定接地 |
| PWMB | PB8 (TIM4_CH3) | 右电机 PWM |
| BIN1 | PB9 (GPIO 推挽) | 右电机方向 |
| BIN2 | GND | 低侧固定接地 |
| STBY | **VCC (5V)** | 硬拉高，始终使能 |
| VM | 7.4V（稳压模块） | 电机驱动动力电 |
| VCC | 5V（稳压模块） | 芯片逻辑电源 |
| GND | GND（共地） | 参考地 |

### TB6612FNG 控制真值表（STBY=VCC 始终使能，AIN2/BIN2=GND）

| AIN1/BIN1 (DIR) | PWMA/PWMB | 输出 | 电机状态 |
|---|---|---|---|
| H (1) | PWM 占空比 | H 桥正向驱动 | **正转** |
| L (0) | PWM 占空比 | H 桥反向驱动 | **反转** |
| X | 0 (CCR=0) | 下管导通对地 | **刹车** |

> A/B 两通道逻辑一致。单独刹停某电机写 PWM=0 即可，不影响另一通道。

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

### 空闲引脚

| 引脚 | 原用途 | 现在状态 |
|---|---|---|
| PB0 | 左电机 EN/STBY | **已释放**，可用于其他功能 |
| PA5 | 右电机 EN/STBY | **已释放**，可用于其他功能 |

### 电机规格

| 参数 | 值 |
|---|---|
| 型号 | MG310 直流减速电机（集成 AB 相编码器） |
| 额定电压 | 7.4V（2S 锂电） |
| 减速比 | 1:20 |
| 编码器 | 霍尔 A/B 双相，13 PPR |
| 编码器供电 | 5V |
| 满圈计数 | 13 × 20 = 260 CPR（实测有效值，理论4×待排查） |

### 驱动芯片规格（TB6612FNG）

| 参数 | 值 |
|---|---|
| VM 范围 | 4.5 ~ 13.5V |
| VCC 范围 | 3.3 / 5V |
| 持续电流 | 1.2A |
| 峰值电流 | 3.2A |
| PWM 频率 | 最高 100kHz，当前用 20kHz |
| 保护 | 过流/过热/欠压锁定 |
| 控制模式 | IN/IN：PWMA=A 路 PWM, AIN1=DIR, AIN2=GND |

## 接线注意事项

1. **单 TB6612FNG**：A 通道驱动左电机，B 通道驱动右电机
2. **STBY 硬接 VCC(5V)**：芯片始终使能，不用 MCU 控制
3. **AIN2/BIN2 硬接 GND**：方向完全由 AIN1/BIN1 的 H/L 控制
4. **单电机启停**：通过 PWM=0（下管导通刹车）实现，不操作 GPIO。`drv_motor_set_enable(0)` 即为 PWM 清零
5. **空闲引脚**：PB0、PA5 已释放，可用作其他外设
6. **编码器上拉**：PA0/PA1/PA6/PA7 已配 IPU（内部上拉），信号弱时外加 10kΩ 上拉到 3.3V
7. **共地**：电池负极、稳压模块 GND、STM32 GND、TB6612 GND、编码器 GND 全部连通
8. **VM 端建议并联** 100μF/25V 电解 + 0.1μF 陶瓷电容（靠近芯片）
9. **PWM 频率 20kHz**，超出人耳范围，无啸叫
10. **TB6612FNG VM 最高 13.5V**，切勿超压
11. **编码器 VCC 为 5V**，切勿接 7.4V — MG310 编码器独立供电，非电机电压

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
