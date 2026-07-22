# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 当前阶段目标

**打通显示域 ↔ 动力域 CAN 通信链路 + 显示域 LVGL 界面。**

电机能转就行（PWM 驱动/编码器读取已实现），不追求转速精度——后期会更换电机。当前重心是让两个 ECU 通过 CAN 正常收发数据。

---

## 最近会话完成工作

### 编码器测试与硬件验证 `[已验证]`

- **TIM 配置**：`TIM_EncoderMode_TI12`（4× 编码器模式），SMS=3、CCER=0000、CCMR1=0101 均已确认正确
- **两根电机均已测试**：
  - 反转时 TIM 正确输出 1040 计数/输出轴圈（=13PPR×4×20 减速比），4× 编码器工作正常
  - 正转时因电机为蜗轮蜗杆减速、具有单向自锁特性，手动拧输出轴时编码器几乎不动（约 260 计数/圈）
- **结论**：软件 4× 编码器配置没有问题，不对称计数是当前电机的机械特性导致
- ENC_CPR 当前设为 260（实测有效值），换电机后改为 1040

### 单 TB6612FNG 适配 `[已完成]`

- 原设计使用两片 TB6612FNG，实际硬件仅一片
- STBY 硬接 VCC(5V) 始终使能，AIN2/BIN2 硬接 GND
- `drv_motor_set_enable()` 改为纯软件启停（PWM=0 刹车）
- 释放引脚：PB0 (原左 EN)、PA5 (原右 EN)

---

## 历史会话完成工作

### 0. CAN 驱动层 Bug 修复（drv_can.c）`[未上板验证]`

| 项目 | 说明 |
|---|---|
| PA11 RX 修正为 IPU | 原配置为 AF_PP（推挽输出），与收发器 RXD 冲突 |
| 波特率 500kbps@36MHz(BS1=9tq) | 采样点≈83.3%，与 F429 对齐 |
| CAN_ABOM | 自动 Bus-Off 恢复 |
| 错误中断 `CAN_IT_ERR` | F1 的 EWG/EPV/BOF 无独立使能位 |
| SCE ISR 简化 | ABOM 已使能，硬件自动恢复 |
| 回调按值传递 | `can_rx_cb(msg)` 消除指针竞态 |

### 1. CAN TX/RX 管道-业务分离 `[未上板验证]`

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
                          解析 0x020/0x021 控制帧 → motor_*.target_*
```

### 2. CAN 协议帧定义

| 帧 | Mode ID | 方向 | 周期 | 状态 |
|---|---|---|---|---|
| 左电机控制帧 | 0x020 | 显域→动力 | RX 事件驱动 | 代码完成，未上板验证 |
| 右电机控制帧 | 0x021 | 显域→动力 | RX 事件驱动 | 代码完成，未上板验证 |
| 左电机状态帧 | 0x110 func=0x00 | 动力→显域 | 10ms | 代码完成，未上板验证 |
| 右电机状态帧 | 0x110 func=0x01 | 动力→显域 | 10ms | 代码完成，未上板验证 |
| 心跳帧 | 0x320 | 动力→显域 | 500ms | 代码完成，未上板验证 |

### 3. 电机驱动实现（drv_motor.c/h）`[已验证编码器，PWM 未上板]`

| 信号 | 左电机 | 外设 | 右电机 | 外设 |
|---|---|---|---|---|
| PWM | PA8 | TIM1_CH1 (20kHz) | PB8 | TIM4_CH3 (20kHz) |
| DIR | PA4 | GPIO | PB9 | GPIO |
| Enc A | PA0 | TIM2_CH1（4× 编码器）| PA6 | TIM3_CH1（4× 编码器）|
| Enc B | PA1 | TIM2_CH2 | PA7 | TIM3_CH2 |

- TB6612FNG 单芯片驱动双电机：A 通道→左，B 通道→右
- STBY 硬接 5V，AIN2/BIN2 硬接 GND，单电机启停 PWM=0 刹车

### 4. PID 转速闭环 `[未上板验证]`

- 位置式 PID + 积分抗饱和，左右独立，5ms 周期
- 默认参数 Kp=2.0, Ki=0.1, Kd=0.5
- 停机策略：`target_speed_enc==0` → PWM=0 + Pid_Reset()

### 5. UART 串口控制台 `[已验证]`

- 115200 8N1，支持 `echo`、`help` 命令
- TX/RX 双队列，ISR 驱动接收 + 主循环消费

### 6. 周期调度 `[已验证]`

- SysTick 中断驱动 8 周期标志位（1ms/5ms/10ms/20ms/100ms/200ms/500ms/1000ms）
- `tpf` 加 `volatile` 修饰（ISR 写入/主循环读取）

---

## 当前工程状态

### 已完成模块

| 模块 | 文件 | 状态 |
|---|---|---|
| CAN 驱动层 | `driver/drv_can.c/h` | 未上板验证 |
| CAN 模块层 | `Mod/Mod_Comm_Can.c/h` | 未上板验证 |
| CAN 任务层 | `task/task_comm_can.c/h` | 未上板验证 |
| 协议定义 | `protocol/CAN_Protocol.h` | 未上板验证 |
| 电机驱动 | `driver/drv_motor.c/h` | 编码器已验证，PWM 未上板 |
| 电机模块 | `Mod/Mod_Motor.c/h` | 未上板验证 |
| 电机任务 | `task/task_motor_ctl.c/h` | 未上板验证 |
| PID 控制器 | `component/pid/pid.c/h` | 未上板验证 |
| 串口驱动 | `driver/drv_usart.c/h` | 已验证 |
| 串口模块 | `Mod/Mod_Usart.c/h` | 已验证 |
| 串口任务 | `task/task_uart.c/h` | 已验证 |
| 系统时钟 | `System/sysclock.c/h` | 已验证 |
| 环形队列 | `component/queue/queue.c/h` | 已验证 |
| 延时 | `System/Delay.c/h` | 已验证 |
| 主循环 | `User/main.c` | 已验证 |

### 编译状态

- armcc V5.06, C99, 优化等级 1，编译通过、无警告无错误

---

## 下一步待做工作（按优先级）

### P0 — CAN 通信上板验证（最高优先级）

这是串联两个 ECU 的关键链路。

**动力域侧（本 ECU）**：
1. 连接 CAN 收发器到 PA11(RX)/PA12(TX)
2. 与显示域 ECU 的 CAN 总线互联（共 CANH/CANL）
3. 两端 120Ω 终端电阻

**验证步骤**：
1. 显示域上电发 0x020（左）/0x021（右）控制帧（50ms 周期）
2. 动力域串口观察 `rx_ctrl_count` 增长
3. 显示域观察 0x110 状态帧（10ms）和 0x320 心跳帧（500ms）

**注意事项**：
- 显示域 `CAN_Protocol.h` 中的 `CAN_HEARTBEAT_ID` mode 为 0x000，应改为 0x320
- `CanCtrlMotor` 结构体线序与显示域实际发送不一致，RX 解析请按裸字节处理

### P1 — 电机 PWM 驱动上板

1. 连接 TB6612FNG 电源（7.4V VM + 5V VCC）和电机
2. 通过 UART 控制台发 `motor left 500` 指令 → 观察电机是否转动
3. 确认 PWM 频率 20kHz、占空比可控

> 当前电机会在后期更换。PWM 只要能驱动电机转即可，不需要精确转速控制。

### P2 — 显示域 LVGL 界面开发

在显示域 ECU（STM32F429）侧开发：
- SPI LCD (ILI9341V) 驱动
- LVGL 移植（FreeRTOS + 触摸 + 显示缓冲）
- 仪表盘界面（车速、转速、电机状态指示）

### P3 — 显示域 ↔ 动力域 闭环联调

- 显示域发控制指令 → 动力域接收并驱动电机
- 动力域上报状态 → 显示域实时刷新 UI

### P4 — 故障保护 `[待实现]`

- IWDG：1s 超时独立看门狗
- CAN 超时停机：`motor_*.last_ctrl_ms` 200ms 超时
- 堵转检测、编码器丢失检测

### P5 — 更换电机

- 电机：转速更准、非蜗杆减速（双向可手动拧动）
- 编码器参数更新：ENC_CPR = PPR × 4 × GEAR_RATIO

---

## 架构设计决策

1. **裸机轮询**：主循环 + SysTick 中断驱动多周期调度
2. **CAN TX 轮询式**：10ms 周期 `Can_Tx_Process`，邮箱满自动重试
3. **CAN RX 中断+队列**：FIFO0 中断接收 → 环形队列 → 主循环消费
4. **管道-业务分离**：CAN 任务做帧收发管道，电机/心跳业务任务做组帧入队
5. **RX 弱符号回调**：`TaskCanMotor_RxCallback` weak → 解耦 CAN 与电机模块
6. **双电机独立 PID**：左右各一个 `PidController`，5ms 周期
7. **单 TB6612FNG**：A 通道→左电机、B 通道→右电机、STBY 硬接 5V
8. **单电机启停**：PWM=0 刹车（TB6612 下管导通），不操作 GPIO

---

## 电机与电源硬件规格

### 电机选型

| 参数 | 当前值（临时） | 换电机后预期 |
|---|---|---|
| 型号 | MG310 蜗杆减速 | TBD（双向无自锁） |
| 额定电压 | 7.4V | 7.4V |
| PPR | 13 | TBD |
| 减速比 | 1:20 | TBD |
| ENC_CPR | 260（手动正转有效值）| PPR×4×减速比 |

### 驱动芯片（TB6612FNG ×1）

| 参数 | 值 |
|---|---|
| VM | 7.4V（稳压模块） |
| VCC / STBY | 5V（稳压模块，始终使能） |
| PWMA/AIN1/AIN2 | PA8/PA4/GND → 左电机 |
| PWMB/BIN1/BIN2 | PB8/PB9/GND → 右电机 |
| PWM 频率 | 20kHz |

### 电源架构

```
7.4V 电池 → 稳压模块 → 7.4V → TB6612 VM
                     → 5V   → TB6612 VCC/STBY + 编码器 VCC
                     → 3.3V → STM32 VDD
                     → GND  → 所有模块共地
```

---

## 已知注意事项

1. **CAN_SELF_ADDR = CAN_ADDR_MOTORBOARD (0x02)**：切勿改回 MAINBOARD
2. **CAN 滤波器全通**：掩码 0，接收所有 29-bit 扩展帧
3. **TX 队列容量 20 帧**：入队失败 `motor_tx_err_count++`，不做丢失降级
4. **CanCtrlMotor 结构体线序与线上不一致**：不要用结构体直接 memcpy 解析
5. **Delay.c 与 sysclock.c 共享 SysTick**：不可同时使用
6. **tpf 必须加 volatile**：ISR 写入/主循环读取，armcc -O1 会缓存
7. **Keil 项目**：`drv_motor.c` 有 `<FileOption>` 自定义对象文件名，需保留
8. **TB6612FNG ×1**：STBY 硬接 5V，空闲引脚 PB0/PA5 可用
9. **编码器 VCC = 5V**，切勿接 7.4V
10. **VM 端并联** 100μF/25V 电解 + 0.1μF 陶瓷（靠近芯片）
11. **PWM 频率 20kHz**，超出人耳范围
12. **UART 中断优先级**：USART1=3 > CAN RX0=5 > CAN SCE=4 > SysTick=15

---

## 主循环调度表

| 周期 | 标志位 | 执行的任务 |
|---|---|---|
| 5ms | `tpf.task_period_5ms` | `Task_Motor_Ctl()` — 编码器更新 + PID 控制 |
| 10ms | `tpf.task_period_10ms` | `Task_Comm_Rx_Can()` + `Task_Comm_Tx_Can()` + `Task_Can_Motor_Updata()` |
| 20ms | `tpf.task_period_20ms` | `Task_Uart_Tx()` + `Task_Uart_Rx()` |
| 500ms | `tpf.task_period_500ms` | `Task_Can_Heartbeat_Updata()` |

## 中断向量

| 中断源 | 向量名 | 处理函数 | 文件 |
|---|---|---|---|
| CAN RX0 | `USB_LP_CAN1_RX0_IRQHandler` | `USB_LP_CAN1_RX0_IRQHandler` | `driver/drv_can.c` |
| CAN SCE | `CAN1_SCE_IRQHandler` | `CAN1_SCE_IRQHandler` | `driver/drv_can.c` |
| SysTick | `SysTick_Handler` | `SysTick_Handler` → `SysClock_Cb()` | `System/sysclock.c` |
| USART1 | `USART1_IRQHandler` | `USART1_IRQHandler`（RXNE+IDLE） | `driver/drv_usart.c` |

## 快速参考

| 定义 | 值 | 说明 |
|---|---|---|
| `CAN_SELF_ADDR` | 0x02 | 动力域设备地址 |
| CAN 波特率 | 500kbps | BS1=9tq, BS2=2tq, SJW=1tq, 采样点≈83.3% |
| `ENC_PPR` | 13 | 临时电机编码器单相 PPR |
| `ENC_GEAR_RATIO` | 20 | 临时减速比 |
| `ENC_CPR` | 260 | 临时有效计数/圈（换电机后改 PPR×4×GEAR） |
| `PWM_MAX` | 999 | 占空比 ±999 |
| PWM 频率 | 20kHz | PSC=3, ARR=899, f=72M/(4×900) |
| Encoder TIM | TIM2(左)/TIM3(右) | TI12 4× 边沿计数 |
| UART 波特率 | 115200 8N1 | PA9(TX)/PA10(RX) |
