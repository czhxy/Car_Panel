# UART 串口控制台命令参考

## 连接参数

| 参数 | 值 |
|---|---|
| 引脚 | PA9 (TX), PA10 (RX) |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 电平 | 3.3V TTL |

## 命令列表

### 电机控制

| 命令 | 参数 | 说明 | 示例 |
|---|---|---|---|
| `motor left <spd>` | `spd`: 转速 rpm×10，支持负数 | 设左电机目标转速 | `motor left 100` |
| `motor right <spd>` | `spd`: 转速 rpm×10，支持负数 | 设右电机目标转速 | `motor right -50` |
| `motor stop` | 无 | 紧急停止双电机（目标转速置零） | `motor stop` |

### 状态查询

| 命令 | 参数 | 说明 | 示例 |
|---|---|---|---|
| `status` | 无 | 打印左右电机当前状态 | `status` |

输出格式：

```
--- Motor Status ---
L spd:<当前>/<目标> ang:<角度> st:<状态标志>
R spd:<当前>/<目标> ang:<角度> st:<状态标志>
```

| 字段 | 单位 | 说明 |
|---|---|---|
| `spd` | rpm×10 | 当前转速 / 目标转速 |
| `ang` | °×10 | 编码器角度 (0–3599) |
| `st` | 十六进制 | bit0=运行中, bit1=已使能, bit7=故障 |

### 调试开关

| 命令 | 参数 | 说明 | 示例 |
|---|---|---|---|
| `enc on` | 无 | 开启编码器调试打印（每 500ms 输出一次状态） | `enc on` |
| `enc off` | 无 | 关闭编码器调试打印 | `enc off` |

### 基础命令

| 命令 | 参数 | 说明 | 示例 |
|---|---|---|---|
| `help` | 无 | 显示命令帮助 | `help` |
| `echo <msg>` | `msg`: 任意文本 | 回显消息（串口链路测试） | `echo hello` |

## 状态标志位

| 位 | 宏 | 含义 |
|---|---|---|
| bit0 (0x01) | `MOTOR_STATUS_RUN` | 电机运转中 |
| bit1 (0x02) | `MOTOR_STATUS_ENABLE` | 驱动器已使能 |
| bit7 (0x80) | `MOTOR_STATUS_FAULT` | 故障 |

## 测试流程

```
1. 接 USB-TTL 到 PA9(TX)/PA10(RX)，打开终端
2. 输入 help → 确认控制台正常
3. 输入 status → 确认初始状态全为零
4. 输入 enc on → 开启编码器打印
5. 手动转动电机 → 观察 ang 值变化
6. 输入 motor left 100 → 设左电机目标转速
7. 观察 spd 当前值是否收敛到目标值（PID 闭环）
8. 输入 motor stop → 停止
9. 输入 enc off → 关闭调试打印
```

## 代码位置

| 组件 | 文件 |
|---|---|
| 命令解析（强定义） | `task/task_uart.c` → `Usart_ParseCommand()` |
| 命令解析（弱默认） | `Mod/Mod_Usart.c` → `__weak Usart_ParseCommand()` |
| RX 处理 | `Mod/Mod_Usart.c` → `Usart_Rx_Process()` |
| TX 处理 | `Mod/Mod_Usart.c` → `Usart_Tx_Process()` |
| 驱动层 ISR | `driver/drv_usart.c` → `USART1_IRQHandler()` |
