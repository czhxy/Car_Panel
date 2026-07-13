# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 本次会话完成工作

### 1. CAN TX/RX 基础设施修复（18 个 Bug 全部修复）

**驱动力层的致命缺陷（drv_can.c）**：
- 补充了缺失的 `CAN_Init(CAN1, &CAN_InitStructure)` 调用 → CAN 外设进入 Normal 模式
- 修正 NVIC 中断通道：滤波器绑定 FIFO0，中断从 `CAN1_RX1_IRQn`（FIFO1）改为 `USB_LP_CAN1_RX0_IRQn`（FIFO0）
- 修正中断处理函数名：`CAN1_RX1_IRQHandler` → `USB_LP_CAN1_RX0_IRQHandler`（匹配启动向量表）
- 修正 `RxMessage` 野指针 → 全局实例 `CanRxMsg RxMessage`
- 新增 `CAN1_SCE_IRQHandler`：处理 Bus-Off 自动恢复 + Error Passive/Error Warning 记录
- 移除未使用的 FIFO1 中断使能

**模块层的逻辑缺陷（Mod_Comm_Can.c）**：
- `TxPack` 从 NULL 指针改为实例 → TX 不再空转
- 发送失败（`CAN_NO_MB` 邮箱满）时不再从队列出队 → 数据保留重试
- `Can_Tx_Process` 补充空队列/空邮箱返回值
- `Can_Tx_Event` 增加 NULL 检查 + `Queue_Put` 返回值检查
- 实现了 `Can_Rx_Event`（中断回调→RX队列入队）
- 实现了 `Can_Rx_Process`（RX队列出队消费）

**系统时钟层的致命缺陷（sysclock.c）**：
- `tp`/`tpf` 从 NULL 指针改为静态实例 → 解决 HardFault
- `SysClock_Cb` 周期判断从 `tpf`（uint8_t 标志位）改为 `tp`（uint64_t 时间戳）→ 周期调度逻辑正确
- `sysclock.h`、`main.h`、`main.c` 中 extern/访问语法全部适配

**协议与工程修复**：
- `CAN_SELF_ADDR` 从 `CAN_ADDR_MAINBOARD` 改为 `CAN_ADDR_MOTORBOARD`
- `task_comm_can.h` 移除了 `#include "task_comm_can.h"` 自包含
- `Project.uvprojx` 移除了 task 组中重复的 `drv_motor.c`
- 4 个文件中的乱码注释（GBK→UTF-8 编码问题）全部修正

### 2. 当前 CAN TX/RX 数据流

```
TX 路径（应用→硬件）：
  应用层 → Can_Tx_Event(CanTxMsg*) → Queue_Put → CanTxQueue（环形缓冲 20×16B）
                                                  ↓
  主循环 10ms → Task_Comm_Tx_Can() → Can_Tx_Process()
                                          ├─ 队列空 → 返回 NoMailBox
                                          ├─ 邮箱满 → 返回 NoMailBox，数据保留
                                          └─ 成功   → CAN_Transmit → Queue_Get 出队

RX 路径（硬件→应用）：
  CAN 中断 → USB_LP_CAN1_RX0_IRQHandler → CAN_Receive → can_rx_cb(msg)
                                              ↓
                                        Can_Rx_Cb → Queue_Put → CanRxQueue
                                                                   ↓
  主循环 10ms → Task_Comm_Rx_Can() → Can_Rx_Process() → Queue_Get 出队消费
```

---

## 当前工程状态

### 已完成模块

| 模块 | 文件 | 状态 |
|---|---|---|
| CAN 驱动层 | `driver/drv_can.c` | **完成** — init、中断、TX、RX、SCE 错误处理 |
| CAN 模块层 | `Mod/Mod_Comm_Can.c` | **完成** — TX/RX 双队列、发送/消费分离 |
| CAN 任务层 | `task/task_comm_can.c` | **框架完成** — Init/Tx/Rx 调度，CAN 协议帧组装/解析待实现 |
| 系统时钟 | `System/sysclock.c` | **完成** — 1/5/10/20/100/200/500/1000ms 周期调度 |
| 主循环 | `User/main.c` | **完成** — 1ms/5ms/10ms/20ms 多周期调度框架 |
| 电机驱动 | `driver/drv_motor.c` | **空壳** — 只有 `drv_motor_init()` 空函数 |
| 电机模块 | `Mod/Mod_Motor.c` | **空壳** — 只有 `Mod_Motor_Init()` 调用驱动初始化 |
| 电机任务 | `task/task_motor_ctl.c` | **空壳** — 函数体为空 |
| 串口驱动 | `driver/drv_usart.c` | **空壳** |
| 串口模块 | `Mod/Mod_Usart.c` | **空壳** |
| 串口任务 | `task/task_uart.c` | **空壳** |
| 协议定义 | `protocol/CAN_Protocol.h` | **完成** — CAN_SELF_ADDR 已正确设为 CAN_ADDR_MOTORBOARD |
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
5. **SCE 中断注册**：Bus-Off 自动恢复、Error Passive/Warning 记录（已实现 ISR，日志记录待补充）

---

## 下一步待做工作（按优先级）

### P0 — CAN 协议帧组装与发送

应该在 `task/task_comm_can.c` 中实现：

1. **心跳帧（0x320, 500ms）**
   ```c
   // 组装帧：设备状态 + 心跳计数
   // 调用 Can_Tx_Event() 入队
   ```

2. **电机状态帧（0x110, 20ms）**
   ```c
   // 组装帧：转速、电流、编码器角度
   // 调用 Can_Tx_Event() 入队
   ```

3. **接收帧解析（0x020 电机控制）**
   ```c
   // 在 Can_Rx_Process() 中解析接收帧
   // 提取目标转速/电流，传给电机控制模块
   ```

   注意：需要匹配 `.\protocol\CAN_Protocol.h` 中的 ID 位域定义和编码函数。

### P1 — 电机驱动实现

在 `driver/drv_motor.c` 中实现：

- **PWM 输出**：TIM1_CH1 (PA8)，H 桥控制，频率建议 20kHz
- **方向控制**：PA4 (DIR) GPIO 输出
- **使能控制**：PB0 (EN) GPIO 输出
- **编码器读取**：TIM2_CH1/CH2 (PA0/PA1)，Encoder Mode，获取转速和角度
- **PID 闭环**：速度环/位置环，增量式或位置式 PID

### P2 — 故障保护

- **IWDG**：1s 超时独立看门狗
- **堵转检测**：500ms 内编码器无变化 → 停机
- **编码器丢失**：100ms 内无脉冲 → 告警
- **CAN 超时停机**：200ms 内未收到控制指令 → 电机停机

### P3 — 串口日志

- `driver/drv_usart.c`：USART1 (PA9/PA10) 初始化，printf 重定向
- `Mod/Mod_Usart.c`：日志输出接口
- `task/task_uart.c`：周期性上报调试信息

---

## 已知注意事项

1. **CAN_SELF_ADDR = CAN_ADDR_MOTORBOARD**：切勿改回 MAINBOARD，否则与显示域通信的源地址会错
2. **Delay.c 与 sysclock.c 共享 SysTick**：`Delay_us/ms` 会直接操作 SysTick 寄存器，sysclock 使用 SysTick 中断。两者不可同时使用。当前主循环使用 `tpf` 周期标志调度，不需要 Delay
3. **注释编码**：文件编码为 UTF-8，所有中文注释使用 `/* ... */` 格式，避免 GBK 乱码
4. **CAN 滤波器是全通**：掩码为 0，接收总线上所有 29-bit 扩展帧。后续可根据协议中的设备地址添加过滤
5. **TX 队列容量 20 帧**：如果 10ms 内入队过多会丢帧，需在 `Can_Tx_Event` 的 `Queue_Put` 失败分支添加告警
6. **RxPack 是全局变量**：`CanRxMsg RxPack` 在 Mod_Comm_Can.c 中定义，`Can_Rx_Event` 和 `Can_Rx_Process` 共用。在中断上下文中 `Can_Rx_Cb` 写入、主循环中 `Can_Rx_Process` 读取，需要确保时序安全（当前队列机制保证了这一点）

---

## 快速参考

### 关键的宏和地址

| 定义 | 值 | 来源 |
|---|---|---|
| `CAN_SELF_ADDR` | 0x02 (MOTORBOARD) | `protocol/CAN_Protocol.h:62` |
| `CAN_Prescaler` | 6 | `driver/drv_can.c:30` |
| CAN 波特率 | 500kbps (42M/6/14) | BS1=11, BS2=2, SJW=1 |
| `TICKS_PER_MS` | 72 | `System/sysclock.c:7`（72MHz 下 1ms=72000 tick） |

### 中断向量与处理函数

| 中断源 | 向量名 | 处理函数 | 文件 |
|---|---|---|---|
| CAN RX0 (FIFO0) | `USB_LP_CAN1_RX0_IRQHandler` | `USB_LP_CAN1_RX0_IRQHandler` | `driver/drv_can.c:72` |
| CAN SCE | `CAN1_SCE_IRQHandler` | `CAN1_SCE_IRQHandler` | `driver/drv_can.c:85` |
| SysTick | `SysTick_Handler` | `SysTick_Handler` → `SysClock_Cb()` | `System/sysclock.c` |

### 周期调度表

| 周期 | 标志位 | 执行的任务 |
|---|---|---|
| 5ms | `tpf.task_period_5ms` | `Task_Motor_Ctl()` |
| 10ms | `tpf.task_period_10ms` | `Task_Comm_Rx_Can()`, `Task_Comm_Tx_Can()` |
| 20ms | `tpf.task_period_20ms` | `Task_Uart_Tx()`, `Task_Uart_Rx()` |
