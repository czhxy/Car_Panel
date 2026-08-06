# HANDOFF.md — 动力域 ECU 当前进度与交接

> 供下一个 AI/开发者接手时快速了解已做的工作、当前状态和下一步任务。总体概览见 `CLAUDE.md`。

---

## 当前阶段目标

**左电机全链路已完成（串口 + PID + CAN），CAN 超时保护就绪。当前仅使用左电机完成验证，右电机代码保留但未接线调参。**

下一步：显示域 ECU 联调，或独立看门狗/堵转检测等故障保护。

---

## 已完成功能（最终状态）

### 电机驱动 `[已验证]`

- **TB6612FNG** 单芯片，A 通道→左，B 通道→右
- STBY 硬接 5V，IN1/IN2 双 GPIO 控制方向（AIN2/BIN2 不再是 GND）
  - 正转: IN1=H, IN2=L
  - 反转: IN1=L, IN2=H
  - 刹车: IN1=L, IN2=L
- 编码器 ICF=0xF 数字滤波（~3.5μs 毛刺抑制）

| 信号 | 左电机 | 外设 | 右电机 | 外设 |
|---|---|---|---|---|
| PWM | PA8 | TIM1_CH1 (20kHz, PSC=3, ARR=899) | PB8 | TIM4_CH3 (20kHz) |
| IN1 | PA4 | GPIO | PB9 | GPIO |
| IN2 | PB0 | GPIO | PA5 | GPIO（未接线） |
| Enc A | PA0 | TIM2_CH1（4× 编码器） | PA6 | TIM3_CH1（4× 编码器） |
| Enc B | PA1 | TIM2_CH2 | PA7 | TIM3_CH2 |

### PID 转速闭环 `[左电机可用]`

- 位置式 PID，dt 缩放（`integral += error × 0.005`），5ms 周期
- 默认 **Kp=1.0, Ki=0.1, Kd=0.3**
- 目标方向改变时自动 Pid_Reset
- 串口在线调参 + VOFA+ FireWater 波形（`vofa l on`）

### CAN 通信 `[已验证]`

- 500kbps，29-bit 扩展帧，BS1=9tq, BS2=2tq, 采样点≈83.3%
- CAN RX：FIFO0 中断 → 环形队列 → 主循环消费（10ms）
- CAN TX：业务组帧入队 → `Can_Tx_Process` 循环排空（最多 8 帧/周期）
- 电机状态帧 20ms 交替发送（左/右各 1 帧/20ms），避免挤占心跳帧
- 心跳帧 0x320 每 500ms 广播
- CAN 控制帧收到后串口回显 `CAN L:<spd>`

### CAN 超时保护 `[已验证]`

- 200ms 无控制帧 → 强制刹车 + 置 FAULT 状态
- CAN 帧恢复后自动清故障、恢复运行
- 串口命令控制不受影响（`last_ctrl_ms` 始终为 0）

### 串口控制台 `[已验证]`

| 命令 | 功能 |
|---|---|
| `motor l/r <spd>` | 设置目标转速（rpm×10） |
| `motor l/r stop` / `motor all stop` | 刹车 |
| `status [l/r]` | 电机状态 |
| `pid l/r show` | 显示 PID 参数 |
| `pid l/r on` / `pid off` | 文本 PID 调试 |
| `pid l/r kp/ki/kd <val>` | 在线调 PID |
| `vofa l/r on` / `vofa off` | VOFA+ 波形（FireWater, 5ms） |
| `echo <msg>` / `help` | 回显/帮助 |

### 模块状态总表

| 模块 | 文件 | 状态 |
|---|---|---|
| CAN 驱动层 | `driver/drv_can.c/h` | 已验证 |
| CAN 模块层 | `Mod/Mod_Comm_Can.c/h` | 已验证（循环排空） |
| CAN 任务层 | `task/task_comm_can.c/h` | 已验证（控制帧 RX + 心跳 TX） |
| 协议定义 | `protocol/CAN_Protocol.h` | 已验证 |
| 电机驱动 | `driver/drv_motor.c/h` | 已验证 |
| 电机模块 | `Mod/Mod_Motor.c/h` | 已验证（PID + CAN 超时） |
| 电机任务 | `task/task_motor_ctl.c/h` | 已验证（VOFA + CAN 状态 TX） |
| PID 控制器 | `component/pid/pid.c/h` | 已验证（dt 缩放 + 方向复位） |
| 串口驱动/模块/任务 | UART 全栈 | 已验证 |
| 系统时钟/队列/延时 | System + component | 已验证 |

### 编译状态

- armcc V5.06, C99, 优化等级 1，编译通过无警告无错误

---

## CAN 帧参考

### 控制帧（上位机→动力域）

| 帧 | CAN ID (hex) | Data (8B, LE) |
|---|---|---|
| 左电机 | `04480800` | speed_enc(I16) angle_enc(I16) 0 0 |

CAN ID 结构：`prio=1, src=1, dst=2, ftype=0, mode=0x020, func=0`

### 状态/心跳帧（动力域→上位机）

| 帧 | CAN ID (hex) | 周期 |
|---|---|---|
| 左电机状态 0x110 | `04844000` | 20ms |
| 右电机状态 0x110 | `04844001` | —（右电机未接线，暂不发送） |
| 心跳 0x320 | `1080C800` | 500ms |

---

## 下一步待做工作

### P1 — 显示域 ECU 联调
用真正的显示域 ECU 替代 PC 上位机，双向验证控制+状态帧。

### P2 — 右电机接线与 PID 调参
参照左电机流程，接 PA5(BIN2) 和 PA6/PA7(编码器)，调右电机 PID。

### P3 — 故障保护增强
- IWDG 独立看门狗
- 堵转检测（速度=0 但 PWM 持续高输出）
- 编码器丢失检测（速度跳变超阈值）

### P4 — 显示域 LVGL 界面 + 整车闭环联调

---

## 关键设计决策

1. **裸机轮询**：主循环 + SysTick 多周期调度
2. **CAN TX 循环排空**：每 10ms 最多发 8 帧，充分利用 3 个 TX 邮箱
3. **CAN RX 中断+队列**：FIFO0 中断 → 环形队列 → 主循环消费
4. **双 GPIO 方向控制**：TB6612 IN1/IN2，正转 H/L、反转 L/H、刹车 L/L
5. **PID 方向反转复位**：目标符号改变时自动 Pid_Reset
6. **CAN 超时安全**：200ms 无帧自动刹车，恢复即重启
7. **单 TB6612FNG**：A 通道→左、B 通道→右，STBY 硬接 5V

---

## 已知注意事项

1. `CAN_SELF_ADDR = 0x02`（MOTORBOARD），切勿改回 MAINBOARD
2. CAN 滤波器全通：掩码 0，接收所有 29-bit 扩展帧
3. TX 队列 20 帧，`Can_Tx_Process` 循环排空最多 8 帧/周期
4. 电机状态帧 20ms 交替（奇数左、偶数右），不挤占心跳帧
5. `CanCtrlMotor` 结构体线序与线上不一致，按裸字节解析
6. `tpf` 必须 `volatile`，ISR 写入/主循环读取
7. TB6612 IN1/IN2 双 GPIO 控制方向，AIN2/BIN2 不再接 GND
8. 编码器 VCC=5V，VM 端并联 100μF+0.1μF
9. UART 中断优先级：USART1=3 > CAN RX0=5 > CAN SCE=4 > SysTick=15
10. PID dt 缩放：`integral += error × 0.005`，Ki 语义为"每秒积分贡献"
11. CAN 超时只对 CAN 控制有效，串口命令不受影响

---

## 主循环调度表

| 周期 | 标志位 | 执行任务 |
|---|---|---|
| 5ms | `tpf.task_period_5ms` | 编码器更新 + PID + CAN 超时检测 + VOFA 输出 |
| 10ms | `tpf.task_period_10ms` | CAN RX + CAN TX（循环排空）+ 电机状态帧（交替） |
| 20ms | `tpf.task_period_20ms` | UART TX + UART RX |
| 500ms | `tpf.task_period_500ms` | 心跳帧 0x320 |

## 快速参考

| 定义 | 值 |
|---|---|
| `CAN_SELF_ADDR` | 0x02 |
| CAN 波特率 | 500kbps, BS1=9tq, BS2=2tq, SP≈83.3% |
| 电机型号 | MG513, 13PPR, 28:1, 额定12V |
| `ENC_CPR` | 1456（输出轴每转计数） |
| `PWM_MAX` | 999, PSC=3, ARR=899, 20kHz |
| Encoder TIM | TIM2(左)/TIM3(右), TI12 4×, ICF=0xF |
| UART | 115200 8N1, PA9/PA10 |
| PID 默认 | Kp=1.0, Ki=0.1, Kd=0.3, dt=0.005 |
| 左电机 IN1/IN2 | PA4/PB0 |
| 右电机 IN1/IN2 | PB9/PA5（未接线） |
| CAN 超时 | 200ms |
| CAN 超时故障码 | `MOTOR_ERROR_CAN_TIMEOUT = 0x0001` |
| `CAN_HEARTBEAT` 帧 ID | `0x1080C800` |
| `CAN_CTRL_L` 帧 ID | `0x04480800` |
| `CAN_CTRL_R` 帧 ID | `0x04480840` |
