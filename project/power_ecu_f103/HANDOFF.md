# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 本次会话完成工作

### 0. 双电机改造（单电机 → 左右双电机）`[未验证]`

- **驱动层**：`drv_motor.c/h` 重构，所有函数增加 `motor_id` 参数（`MOTOR_ID_LEFT=0`/`MOTOR_ID_RIGHT=1`）
- **右电机硬件**：TIM4_CH3 (PB8) PWM + TIM3 (PA6/PA7) 编码器 + PB9 DIR + PA5 EN
- **模块层**：`motor_left`/`motor_right` 两个独立 `Motor_Struct` 实例，`motor` 宏仍指向 `motor_left`（向后兼容）
- **CAN 接收**：新增 `MODE_ID_CTRL_RF` (0x021) 解析 → 写入 `motor_right.target_*`
- **CAN 发送**：0x110 状态帧每 10ms 发两帧——func_field=0x00(左)、func_field=0x01(右)
- **心跳帧**：status/error_code 取左右电机的 OR 聚合

### 1. CAN 驱动层关键 Bug 修复（drv_can.c）`[未验证]`

- **PA11 RX 脚修正为 IPU 输入**：原配置与 TX 一样设成 AF_PP（推挽输出），输出驱动器与收发器 RXD 冲突，RX 完全失效
- **波特率修正为 500kbps@36MHz(BS1 9tq)**：原来按 42MHz(BS1 11tq) 计算，实为 428.6kbps，与 F429 对不上
- **增加 CAN_ABOM**：自动 Bus-Off 恢复，单节点无 ACK 也会触发 BusOff
- **错误中断改用 `CAN_IT_ERR`**：F1 的 EWG/EPV/BOF 没有独立使能位，`CAN_ITConfig(EWG/EPV/BOF)` 写的是保留位，SCE 中断永远不进
- **SCE ISR 简化**：ABOM 已使能，硬件自动恢复，软件只需读 ESR、清 ERRIE
- **回调改为按值传递**：`can_rx_cb(msg)` 传结构体副本，消除指针竞态

### 2. 模块层重构 — TX/RX 职责分离 `[未验证]`

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
                          解析 0x020 控制帧 → motor_left.target_*
                              解析 0x021 控制帧 → motor_right.target_*
                              解析 0x080 查询帧 → 串口回显
```

**模块-头文件改进**：
- `CAN_Protocol.h`：新增 `CanStatusMotor` 结构体（0x110 状态帧载荷），给 `CanCtrlMotor` 加线序警告注释
- `Mod_Motor.h`：`Motor_Struct` 扩展完整字段（target/cur/status/计数/时间戳），加状态位宏和 `motor_left`/`motor_right` extern 实例
- `Mod_Motor.c`：定义 `motor_left`/`motor_right` 两个全局实例
- `Mod_Comm_Can.c`：加 TX/RX 错误计数器，`Can_Rx_Process` 中调用弱符号 `TaskCanMotor_RxCallback`
- `sysclock.h`：补 `sysclock_get_ms()` 声明

### 3. CAN 协议帧组装与解析 `[未验证]`

| 帧 | Mode ID | 方向 | 周期 | 处理位置 | 验证 |
|---|---|---|---|---|---|
| 左电机控制帧 | 0x020 | 显示域→动力域 | RX 事件驱动 | `TaskCanMotor_RxCallback()` → `motor_left` | 未验证 |
| 右电机控制帧 | 0x021 | 显示域→动力域 | RX 事件驱动 | `TaskCanMotor_RxCallback()` → `motor_right` | 未验证 |
| 左电机状态帧 | 0x110 (func=0x00) | 动力域→显示域 | 10ms | `Task_Can_Motor_Updata()` | 未验证 |
| 右电机状态帧 | 0x110 (func=0x01) | 动力域→显示域 | 10ms | `Task_Can_Motor_Updata()` | 未验证 |
| 查询回显 | 0x080 | 双向 | RX 事件驱动 | `TaskCanMotor_RxCallback()` | 未验证 |
| 心跳帧 | 0x320 | 动力域→显示域 | 500ms | `Task_Can_Heartbeat_Updata()` | 未验证 |

### 4. 电机驱动实现（drv_motor.c/h）— 双电机 `[未验证]`

- **选型**：MG310 直流减速电机 ×2（7.4V, 1:30, 11 PPR AB 相编码器）
- **驱动芯片**：DRV8833 ×2（7.4V 在 2.7~10.8V 最佳区间，堵转 1.85A 在 2A 峰值内，内置过流/过热/欠压保护）

| 信号 | 左电机 | 外设 | 右电机 | 外设 |
|---|---|---|---|---|
| PWM | PA8 | TIM1_CH1 | PB8 | TIM4_CH3 |
| DIR | PA4 | GPIO | PB9 | GPIO |
| EN | PB0 | GPIO | PA5 | GPIO |
| Enc A | PA0 | TIM2_CH1 | PA6 | TIM3_CH1 |
| Enc B | PA1 | TIM2_CH2 | PA7 | TIM3_CH2 |

- **PWM**：两路均为 20kHz（PSC=3, ARR=899，TIM1_CH1 用于左，TIM4_CH3 用于右）
- **编码器**：均为 TI12 4× 边沿计数，1320 脉冲/转
- **转速计算**：`rpm×10 = delta_count × 1000 / 11`（5ms 差分）
- **角度计算**：`°×10 = (pos % 1320) × 3600 / 1320`
- **API 统一**：所有驱动函数通过 `motor_id`（`MOTOR_ID_LEFT=0`/`MOTOR_ID_RIGHT=1`）区分
- `motor_left`/`motor_right` 两个独立 `Motor_Struct` 实例，`motor` 宏仍指向 `motor_left`（向后兼容）
- `Mod_Motor_Update()` 每 5ms 依次更新左右编码器实测值

### 5. UART 驱动完善 `[未验证]`

- `USART_Mode` 从 `Tx|Tx` 修正为 `Tx|Rx`
- GBK 乱码注释全部修正为 UTF-8
- 新增 `Usart_SendByte/Usart_SendData/Usart_SendString` 轮询 TX 函数（链路验证打印用）
- `Task_Uart_Init` 现在真正调用 `Mod_Usart_Init()` 初始化 USART1

---

## 当前工程状态

### 已完成模块

| 模块 | 文件 | 实现状态 | 验证状态 |
|---|---|---|---|
| CAN 驱动层 | `driver/drv_can.c` | 完成 | 未验证 |
| CAN 模块层 | `Mod/Mod_Comm_Can.c` | 完成 | 未验证 |
| CAN 任务层 | `task/task_comm_can.c` | 完成 | 未验证 |
| 协议定义 | `protocol/CAN_Protocol.h` | 完成 | 未验证 |
| 系统时钟 | `System/sysclock.c` | 完成 | 已验证 |
| 主循环 | `User/main.c` | 完成 | 已验证 |
| 电机模块 | `Mod/Mod_Motor.c/h` | 完成 | 未验证 |
| 电机任务 | `task/task_motor_ctl.c` | 完成 | 未验证 |
| 电机驱动 | `driver/drv_motor.c` | 完成 | 未验证 |
| 串口驱动 | `driver/drv_usart.c` | 完成 | 未验证 |
| 串口模块 | `Mod/Mod_Usart.c` | 空壳 | — |
| 串口任务 | `task/task_uart.c` | 框架 | — |
| 环形队列 | `component/queue/queue.c` | 完成 | 已验证 |
| 延时 | `System/Delay.c` | 可用 | 已验证 |

### 当前编译状态

- ✅ **已验证**：armcc V5.06, C99, 优化等级 1，正常编译通过、无警告无错误
- ❌ **未验证**：所有硬件功能（CAN 收发、PWM 输出、编码器读数、串口通信）均未上板测试

### 验证状态说明

| 标签 | 含义 |
|---|---|
| ✅ 已验证 | 代码通过编译或软件逻辑已确认正确 |
| ❌ 未验证 | 代码写完但未在目标硬件上实际运行测试 |

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

### P0 — 电机驱动实现 `[已实现 未验证]`

| 信号 | 左电机 | 外设 | 右电机 | 外设 |
|---|---|---|---|---|
| PWM | PA8 | TIM1_CH1 | PB8 | TIM4_CH3 |
| DIR | PA4 | GPIO | PB9 | GPIO |
| EN | PB0 | GPIO | PA5 | GPIO |
| Enc A | PA0 | TIM2_CH1 | PA6 | TIM3_CH1 |
| Enc B | PA1 | TIM2_CH2 | PA7 | TIM3_CH2 |

- **PWM 频率**：两路均为 20kHz（PSC=3, ARR=899）
- **编码器**：均为 TI12 4× 边沿计数，1320 脉冲/转
- 每 5ms `drv_motor_update(motor_id)` 刷新对应电机实测值
- `cur_current` 暂为 0（无采样电阻），`temperature` 暂为 0

### P1 — 故障保护 `[待实现]`

- **IWDG**：1s 超时独立看门狗
- **CAN 超时停机**：利用 `motor_left.last_ctrl_ms` / `motor_right.last_ctrl_ms` 各 200ms 超时检测
- **堵转检测**：500ms 内编码器无变化 → 停机
- **编码器丢失**：100ms 内无脉冲 → 告警
- 上述保护均需对左右电机分别实现

### P2 — 串口日志完善 `[待实现]`

- `Mod/Mod_Usart.c`：日志输出接口
- `task/task_uart.c`：周期性上报调试信息

### P3 — 显示域同步 `[待实现]`

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
8. **DRV8833 VM 最高 10.8V**，切勿接 12V。编码器 VCC 为 5V，切勿接 7.4V/12V
9. **Keil 项目**：`drv_motor.c` 在项目文件中配置了 `<FileOption>` 内自定义对象文件名（Keil IDE 自动展开），需保留不做回退

---

## 电机与电源硬件规格

### 电机选型

| 型号 | MG310 直流减速电机 |
|---|---|
| 额定电压 | 7.4V（2S 锂电） |
| 减速比 | 1:30 |
| 编码器 | 霍尔 A/B 双相，11 PPR |
| 编码器供电 | 5V |
| 满圈计数 | 11 × 30 × 4 = 1320 计数/转（TIM2 4× 边沿） |

### 电机驱动芯片

| 型号 | DRV8833 |
|---|---|
| 供电范围 | VM 2.7~10.8V, VCC 3.3/5V |
| 持续/峰值电流 | 1.5A / 2A |
| 保护 | 过流/过热/欠压锁定，逐周期保护 |
| 控制接口 | IN/IN 模式：AIN1=PWM, AIN2=DIR, nSLEEP=EN |

### 接线表（双电机，每台 MG310 六线引出）

| 线色/标识 | 左电机 | 右电机 | 电压 |
|---|---|---|---|
| M+ | DRV8833(L) AOUT1 | DRV8833(R) BOUT1 | 7.4V（通过 H 桥） |
| M- | DRV8833(L) AOUT2 | DRV8833(R) BOUT2 | 7.4V（通过 H 桥） |
| VCC | 稳压模块 5V | 稳压模块 5V | 5V |
| GND | GND | GND | 0V |
| A | PA0 (TIM2_CH1) | PA6 (TIM3_CH1) | 信号/上拉 |
| B | PA1 (TIM2_CH2) | PA7 (TIM3_CH2) | 信号/上拉 |

### DRV8833 控制接线

| DRV8833 引脚 | 左电机 STM32 引脚 | 右电机 STM32 引脚 | 功能 |
|---|---|---|---|
| IN1 | PA8 (TIM1_CH1) | PB8 (TIM4_CH3) | PWM 输入 |
| IN2 | PA4 (GPIO) | PB9 (GPIO) | 方向控制 |
| nSLEEP | PB0 (GPIO) | PA5 (GPIO) | 使能/休眠 |
| VM | 电池 7.4V | 电池 7.4V | 电机电源 |
| VCC | 5V | 5V | 逻辑电源 |
| OUT1 | M+ (左) | M+ (右) | 电机输出 |
| OUT2 | M- (左) | M- (右) | 电机输出 |

### 电源树

```
7.4V 电池（2S 锂电）
│
├── 稳压模块 IN
│   ├── 3.3V → STM32F103
│   └── 5V  → 编码器 VCC
│           → DRV8833 VCC（逻辑电源）
│
└── 电池 7.4V → DRV8833(L) VM / DRV8833(R) VM（电机驱动电源）
    ├── DRV8833(L) AOUT1/AOUT2 → MG310(左) M+/M-
    └── DRV8833(R) BOUT1/BOUT2 → MG310(右) M+/M-
```

### 保护建议

- PTC 自恢复保险丝 500mA（电池 → DRV8833 VM 之间）
- 100μF/25V 电解 + 0.1μF 陶瓷（VM 对地，靠近芯片）
- 软件层：PWM 上限 80%、反转前刹车 10ms、500ms 堵转检测（待实现）

---

## 快速参考

### 关键的宏和地址

| 定义 | 值 | 来源 |
|---|---|---|
| `CAN_SELF_ADDR` | 0x02 (MOTORBOARD) | `protocol/CAN_Protocol.h:62` |
| `CAN_Prescaler` | 6 | `driver/drv_can.c:42` |
| CAN 波特率 | 500kbps (36M/6/12) | BS1=9, BS2=2, SJW=1, 采样点≈83.3% |
| `TICKS_PER_MS` | 72 | `System/sysclock.c:7`（72MHz 下 1ms=72000 tick） |
| `ENC_PPR` | 11 | `driver/drv_motor.h:7`（MG310 编码器单相每转脉冲） |
| `ENC_GEAR_RATIO` | 30 | `driver/drv_motor.h:8`（MG310 减速比 1:30） |
| `ENC_CPR` | 1320 | `driver/drv_motor.h:9`（满圈计数 = 11×30×4） |
| `PWM_MAX` | 999 | `driver/drv_motor.h:12`（占空比 -999~+999） |
| PWM 频率 | 20kHz | TIM1_CH1(左) + TIM4_CH3(右), PSC=3, ARR=899, f=72M/(4×900) |
| Encoder TIM | TIM2(左) / TIM3(右) | TI12 4× 边沿计数，1320 CPR |

### 中断向量与处理函数

| 中断源 | 向量名 | 处理函数 | 文件 |
|---|---|---|---|
| CAN RX0 (FIFO0) | `USB_LP_CAN1_RX0_IRQHandler` | `USB_LP_CAN1_RX0_IRQHandler` | `driver/drv_can.c` |
| CAN SCE | `CAN1_SCE_IRQHandler` | `CAN1_SCE_IRQHandler` | `driver/drv_can.c` |
| SysTick | `SysTick_Handler` | `SysTick_Handler` → `SysClock_Cb()` | `System/sysclock.c` |

### 周期调度表

| 周期 | 标志位 | 执行的任务 |
|---|---|---|
| 5ms | `tpf.task_period_5ms` | `Task_Motor_Ctl()` — 刷新左右编码器实测值 |
| 10ms | `tpf.task_period_10ms` | `Task_Comm_Rx_Can()`, `Task_Comm_Tx_Can()`, `Task_Can_Motor_Updata()` — 左右电机状态帧各一(0x110) |
| 20ms | `tpf.task_period_20ms` | `Task_Uart_Tx()`, `Task_Uart_Rx()` |
| 500ms | `tpf.task_period_500ms` | `Task_Can_Heartbeat_Updata()` (0x320 心跳帧) |
