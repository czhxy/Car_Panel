# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 当前阶段目标

**双电机 PID 转速闭环调试。**

编码器已通过验证（双路均正常），PWM 驱动可用，串口控制台就绪。当前重心是通过串口调试两路电机的 PID 参数，使转速稳定跟随目标值。

---

## 最近会话完成工作

### 编码器测试与硬件验证（最终结论） `[已验证]`

- **根因发现**：编码器异常是因为编码器模块和 STM32 未共地。连接共地后双路编码器均正常工作。
- **TIM 配置**：`TIM_EncoderMode_TI12`（4× 编码器模式），SMS=3、CCER=0000、CCMR1=0101（无滤波）均已确认正确。
- **双路编码器**：
  - 左电机（TIM2, PA0/PA1）：编码模式 TI12，正转计数增加，反转计数减少
  - 右电机（TIM3, PA6/PA7）：编码模式 TI12，正转计数增加，反转计数减少
  - 无串扰：各自独立工作，拧一个电机仅对应 TIM 计数器变化
- **结论**：双路 4× 编码器软硬件完全正常，共地是必要条件。

### 电机型号切换与参数 `[已完成]`

- 当前电机：**MG513**，13 PPR，28:1 减速比，额定 12V
- ENC_CPR = 13 × 4 × 28 = **1456 计数/输出轴圈**
- 增设宏切换开关 `MOTOR_MODEL`（`driver/drv_motor.h`）：
  - `MOTOR_MODEL_MG513`（当前）：PPR=13, 减速比=28, 额定=12V
  - `MOTOR_MODEL_MG310`（之前）：PPR=13, 减速比=20, 额定=7.4V
  - 切换只需改一行 `#define MOTOR_MODEL MOTOR_MODEL_MG310`

### 串口命令精简 `[已完成]`

- 移除编码器调试命令（`enc test`、`enc diag`、`enc on/off`、`regs`）
- 保留下来的命令集：

| 命令 | 功能 |
|---|---|
| `motor l/r <spd>` | 设置目标转速（rpm×10），正转正数、反转负数 |
| `motor l/r stop` | 单电机刹车 |
| `motor all stop` | 双电机刹车 |
| `status [l/r]` | 显示电机状态（speed/angle/status） |
| `pid l/r show` | 显示 PID 参数 (Kp/Ki/Kd/integ/out) |
| `pid l/r on` | 开启 PID 调试打印（100ms 周期） |
| `pid off` | 关闭 PID 调试 |
| `pid l/r kp/ki/kd <val>` | 在线调整 PID 增益 |
| `echo <msg>` | 回显（串口测试） |
| `help` | 帮助信息 |

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

### 4. PID 转速闭环 `[待调试]`

- 位置式 PID + 积分抗饱和，左右独立，5ms 周期
- 默认参数 Kp=2.0, Ki=0.1, Kd=0.5
- 停机策略：`target_speed_enc==0` → PWM=0 + Pid_Reset()
- 可通过串口实时调参（`pid l/r kp/ki/kd`、`pid l/r on` 观察响应）

### 5. UART 串口控制台 `[已验证]`

- 115200 8N1，支持电机控制、PID 调参、状态查询等命令
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

### P0 — 双电机 PID 转速闭环调试（最高优先级）

**当前状态**：编码器双路验证通过，PWM 驱动可用，PID 代码就绪但未调参。

**调试流程**：
1. 左电机先独立调，`motor l <spd>` 设定目标，`pid l on` 观察响应
2. 通过 `pid l kp/ki/kd <val>` 在线调整增益
3. 调好后 `pid l show` 记录参数，同样过程调整右电机
4. 注意：当前电机额定 12V，但供电可能是 7.4V，会影响最大转速

**预期效果**：
- 阶跃响应：触发后转速迅速跟上目标，超调 < 20%
- 稳态误差：< 5%
- 抗扰：施加阻力时转速自动回升

### P1 — CAN 通信上板验证

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

| 参数 | 当前值（MG513） | 备用（MG310） |
|---|---|---|
| 型号 | MG513 | MG310 蜗杆减速 |
| 额定电压 | 12V | 7.4V |
| PPR | 13 | 13 |
| 减速比 | 1:28 | 1:20 |
| ENC_CPR | 13×4×28 = 1456 | 13×4×20 = 1040 |
| RPM_FACTOR | 120000 | 120000 |

> 在 `driver/drv_motor.h` 中修改 `MOTOR_MODEL` 宏即可切换电机型号，PPR/减速比/CPR 自动适配。

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
| `ENC_PPR` | 13 | MG513 编码器单相 PPR |
| `ENC_GEAR_RATIO` | 28 | MG513 减速比 1:28 |
| `ENC_CPR` | 1456 | 13×4×28，输出轴每转计数 |
| `PWM_MAX` | 999 | 占空比 ±999 |
| PWM 频率 | 20kHz | PSC=3, ARR=899, f=72M/(4×900) |
| Encoder TIM | TIM2(左)/TIM3(右) | TI12 4× 边沿计数 |
| UART 波特率 | 115200 8N1 | PA9(TX)/PA10(RX) |
