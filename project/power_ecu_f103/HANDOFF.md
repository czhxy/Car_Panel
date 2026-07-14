# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 本次会话完成工作

### 1. CAN 驱动层关键 Bug 修复（drv_can.c）

- **PA11 RX 脚修正为 IPU 输入**：原配置与 TX 一样设成 AF_PP（推挽输出），输出驱动器与收发器 RXD 冲突，RX 完全失效
- **波特率修正为 500kbps@36MHz(BS1 9tq)**：原来按 42MHz(BS1 11tq) 计算，实为 428.6kbps，与 F429 对不上
- **增加 CAN_ABOM**：自动 Bus-Off 恢复，单节点无 ACK 也会触发 BusOff
- **错误中断改用 `CAN_IT_ERR`**：F1 的 EWG/EPV/BOF 没有独立使能位，`CAN_ITConfig(EWG/EPV/BOF)` 写的是保留位，SCE 中断永远不进
- **SCE ISR 简化**：ABOM 已使能，硬件自动恢复，软件只需读 ESR、清 ERRIE
- **回调改为按值传递**：`can_rx_cb(msg)` 传结构体副本，消除指针竞态

### 2. 模块层重构 — TX/RX 职责分离

**设计原则**：CAN 任务层只做 CAN 帧的管道操作（RX 从队列取帧→分发给弱符号回调、TX 从队列取帧→推送到硬件邮箱）。电机/心跳等实体数据的组帧和入队动作由对应的业务任务负责。

```
TX 路径（业务→硬件）：
  电机任务(10ms) → Task_Can_Motor_Updata()  ─┐
  CAN 任务(500ms)→ Task_Can_Heartbeat_Updata()─┤
                                                 ↓
                                      Can_Tx_Event() → TX 队列
                                                         ↓
  CAN 任务(10ms) → Task_Comm_Tx_Can() → Can_Tx_Process() → CAN_Transmit

RX 路径（硬件→业务）：
  CAN 中断 → USB_LP_CAN1_RX0_IRQHandler → Can_Rx_Cb(msg) → RX 队列
                                                                ↓
  CAN 任务(10ms) → Task_Comm_Rx_Can() → Can_Rx_Process()
                                           ↓
                                   TaskCanMotor_RxCallback(弱符号重写)
                                           ↓
                              解析 0x020 控制帧 → motor.target_*
                              解析 0x080 查询帧 → 串口回显
```

**模块-头文件改进**：
- `CAN_Protocol.h`：新增 `CanStatusMotor` 结构体（0x110 状态帧载荷），给 `CanCtrlMotor` 加线序警告注释
- `Mod_Motor.h`：`Motor_Struct` 扩展完整字段（target/cur/status/计数/时间戳），加状态位宏和 `extern` 实例
- `Mod_Motor.c`：定义全局 `Motor_Struct motor = {0}`
- `Mod_Comm_Can.c`：加 TX/RX 错误计数器，`Can_Rx_Process` 中调用弱符号 `TaskCanMotor_RxCallback`
- `sysclock.h`：补 `sysclock_get_ms()` 声明

### 3. CAN 协议帧组装与解析（已完成）

| 帧 | Mode ID | 方向 | 周期 | 处理位置 |
|---|---|---|---|---|
| 电机控制帧 | 0x020 | 显示域→动力域 | RX 事件驱动 | `task_comm_can.c:TaskCanMotor_RxCallback()` |
| 电机状态帧 | 0x110 | 动力域→显示域 | 10ms | `task_motor_ctl.c:Task_Can_Motor_Updata()` |
| 查询回显 | 0x080 | 双向 | RX 事件驱动 | `task_comm_can.c:TaskCanMotor_RxCallback()` |
| 心跳帧 | 0x320 | 动力域→显示域 | 500ms | `task_comm_can.c:Task_Can_Heartbeat_Updata()` |

### 4. UART 驱动完善

- `USART_Mode` 从 `Tx|Tx` 修正为 `Tx|Rx`
- GBK 乱码注释全部修正为 UTF-8
- 新增 `Usart_SendByte/Usart_SendData/Usart_SendString` 轮询 TX 函数（链路验证打印用）
- `Task_Uart_Init` 现在真正调用 `Mod_Usart_Init()` 初始化 USART1

---

## 当前工程状态

### 已完成模块

| 模块 | 文件 | 状态 |
|---|---|---|
| CAN 驱动层 | `driver/drv_can.c` | **完成** — init、中断、TX、RX、SCE 错误处理、ABOM |
| CAN 模块层 | `Mod/Mod_Comm_Can.c` | **完成** — TX/RX 双队列、按值传递、弱符号回调、错误计数 |
| CAN 任务层 | `task/task_comm_can.c` | **完成** — RX 解析(0x020/0x080)、心跳帧 TX(0x320)、帧管道调度 |
| 协议定义 | `protocol/CAN_Protocol.h` | **完成** — 新增 CanStatusMotor、线序警告注释 |
| 系统时钟 | `System/sysclock.c` | **完成** — 1/5/10/20/100/200/500/1000ms 周期调度 |
| 主循环 | `User/main.c` | **完成** — 多周期调度框架 + NVIC 分组 |
| 电机模块 | `Mod/Mod_Motor.c/h` | **结构完成** — 全局 motor 实例、完整字段定义、状态位宏 |
| 电机任务 | `task/task_motor_ctl.c` | **部分完成** — `Task_Can_Motor_Updata()` 组装 0x110 状态帧 |
| 串口驱动 | `driver/drv_usart.c` | **完成** — USART1 初始化(115200 8N1)、TX 轮询函数 |
| 串口模块 | `Mod/Mod_Usart.c` | **空壳** |
| 串口任务 | `task/task_uart.c` | **框架** — Init 已接 Mod_Usart_Init，Tx/Rx 待实现 |
| 电机驱动 | `driver/drv_motor.c` | **空壳** — 只有 `drv_motor_init()` 空函数 |
| 环形队列 | `component/queue/queue.c` | **完成** — 无 bug |
| 延时 | `System/Delay.c` | 可用，但注意会覆盖 SysTick 寄存器 |

### 当前编译状态

- 应可正常编译通过，无警告无错误
- 编译器：armcc V5.06, C99, 优化等级 1
- 硬件功能待上板验证

---

## 架构设计决策

1. **裸机轮询**：无 RTOS，主循环 + SysTick 中断驱动多周期调度
2. **CAN TX 轮询式**：非中断驱动发送，主循环 10ms 周期调用 `Can_Tx_Process`，邮箱满时自动重试
3. **CAN RX 中断+队列**：FIFO0 中断接收 → 环形队列缓冲 → 主循环消费（解耦 ISR 和业务）
4. **单滤波器全通**：掩码 0，接收所有 CAN 消息，后续可在应用层过滤
5. **SCE 中断注册**：使用 `CAN_IT_ERR` 统一使能错误中断（非 EWG/EPV/BOF 保留位），ABOM 硬件自动 Bus-Off 恢复
6. **管道-业务分离**：CAN 任务层只负责帧收发（TX 队列消费、RX 队列分派），电机/心跳等实体数据的组帧入队由各自业务任务负责，避免 CAN 任务膨胀
7. **RX 弱符号回调**：`TaskCanMotor_RxCallback` 在 `Mod_Comm_Can.c` 中声明为 weak，`task_comm_can.c` 提供强实现，实现 CAN 模块与电机模块解耦

---

## 下一步待做工作（按优先级）

### P0 — 电机驱动实现

在 `driver/drv_motor.c` 中实现：

- **PWM 输出**：TIM1_CH1 (PA8)，H 桥控制，频率建议 20kHz
- **方向控制**：PA4 (DIR) GPIO 输出
- **使能控制**：PB0 (EN) GPIO 输出
- **编码器读取**：TIM2_CH1/CH2 (PA0/PA1)，Encoder Mode，获取转速和角度
- 填充 `motor.cur_speed_enc`、`motor.cur_current`、`motor.cur_angle_enc`、`motor.temperature`
- 设置 `motor.status`（MOTOR_STATUS_RUN/ENABLE）

### P1 — 故障保护

- **IWDG**：1s 超时独立看门狗
- **CAN 超时停机**：利用 `motor.last_ctrl_ms` 做 200ms 超时检测
- **堵转检测**：500ms 内编码器无变化 → 停机
- **编码器丢失**：100ms 内无脉冲 → 告警

### P2 — 串口日志完善

- `Mod/Mod_Usart.c`：日志输出接口
- `task/task_uart.c`：周期性上报调试信息

### P3 — 显示域同步

- 将 `CanStatusMotor` 同步到 `display_ecu_f429/protocol/CAN_Protocol.h`
- 修正显示域心跳 `CAN_HEARTBEAT_ID` 的 mode 0x000 → 0x320

---

## 已知注意事项

1. **CAN_SELF_ADDR = CAN_ADDR_MOTORBOARD**：切勿改回 MAINBOARD
2. **Delay.c 与 sysclock.c 共享 SysTick**：`Delay_us/ms` 会直接操作 SysTick 寄存器，sysclock 使用 SysTick 中断。两者不可同时使用
3. **注释编码**：文件编码为 UTF-8，所有中文注释使用 `/* ... */` 格式
4. **CAN 滤波器是全通**：掩码为 0，接收总线上所有 29-bit 扩展帧
5. **TX 队列容量 20 帧**：入队失败时 `event_err_count.motor_tx_err_count++` 记录，不做丢失降级
6. **管道-业务分离**：新增 CAN 协议帧时应遵循——组帧+入队放业务任务，TX 队列出队+硬件发送放 CAN 任务
7. **CanCtrlMotor 结构体与线上线序不一致**：显示域实际发送 `[speed, speed, angle, angle, 0, 0, 0, 0x03]`，不要用结构体直接 memcpy 解析，收发请按裸字节处理

---

## 快速参考

### 关键的宏和地址

| 定义 | 值 | 来源 |
|---|---|---|
| `CAN_SELF_ADDR` | 0x02 (MOTORBOARD) | `protocol/CAN_Protocol.h:62` |
| `CAN_Prescaler` | 6 | `driver/drv_can.c:42` |
| CAN 波特率 | 500kbps (36M/6/12) | BS1=9, BS2=2, SJW=1, 采样点≈83.3% |
| `TICKS_PER_MS` | 72 | `System/sysclock.c:7`（72MHz 下 1ms=72000 tick） |

### 中断向量与处理函数

| 中断源 | 向量名 | 处理函数 | 文件 |
|---|---|---|---|
| CAN RX0 (FIFO0) | `USB_LP_CAN1_RX0_IRQHandler` | `USB_LP_CAN1_RX0_IRQHandler` | `driver/drv_can.c` |
| CAN SCE | `CAN1_SCE_IRQHandler` | `CAN1_SCE_IRQHandler` | `driver/drv_can.c` |
| SysTick | `SysTick_Handler` | `SysTick_Handler` → `SysClock_Cb()` | `System/sysclock.c` |

### 周期调度表

| 周期 | 标志位 | 执行的任务 |
|---|---|---|
| 5ms | `tpf.task_period_5ms` | `Task_Motor_Ctl()` |
| 10ms | `tpf.task_period_10ms` | `Task_Comm_Rx_Can()`, `Task_Comm_Tx_Can()`, `Task_Can_Motor_Updata()` (0x110 状态帧) |
| 20ms | `tpf.task_period_20ms` | `Task_Uart_Tx()`, `Task_Uart_Rx()` |
| 500ms | `tpf.task_period_500ms` | `Task_Can_Heartbeat_Updata()` (0x320 心跳帧) |
