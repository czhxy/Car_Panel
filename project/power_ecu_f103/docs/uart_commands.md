# 串口命令参考 — 动力域 ECU

## 连接参数

| 参数 | 值 |
|---|---|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | 无 |
| 停止位 | 1 |

---

## 电机控制

### 设定转速

```
motor l <spd>       设定左电机目标转速（rpm × 10）
motor r <spd>       设定右电机目标转速
```

| 示例 | 实际转速 | 说明 |
|---|---|---|
| `motor l 50` | 5.0 rpm 正转 | spd = 5.0 × 10 = 50 |
| `motor l 100` | 10.0 rpm 正转 | |
| `motor l -50` | 5.0 rpm 反转 | 负数表示反转 |
| `motor l 0` | 停止 | 与 stop 同效果 |

回显：`L:50`

### 刹车

```
motor l stop        左电机刹车
motor r stop        右电机刹车
motor all stop      双电机同时刹车
```

回显：`L STOP` / `ALL STOP`

---

## 状态查询

### 查看电机状态

```
status              查询双电机状态
status l            仅查询左电机
status r            仅查询右电机
```

输出格式：

```
L spd:156/500 ang:234 st:03
R spd:0/0 ang:0 st:00
```

| 字段 | 含义 |
|---|---|
| `spd` | 当前转速/目标转速（rpm × 10） |
| `ang` | 编码器角度（° × 10） |
| `st` | 状态位（按位组合） |

### 状态位定义

| 位 | 值 | 含义 |
|---|---|---|
| bit0 | 0x01 | 运行中 (RUN) |
| bit1 | 0x02 | 已使能 (ENABLE) |
| bit7 | 0x80 | 故障 (FAULT) |

示例：`st:03` = RUN(0x01) + ENABLE(0x02)，正常运行中。`st:80` = FAULT(0x80)，CAN 超时触发刹车。

---

## PID 调参

### 查看 PID 参数

```
pid l show          显示左电机 PID 参数
pid r show          显示右电机 PID 参数
```

输出：`L PID: Kp=1.000 Ki=0.100 Kd=0.300 integ=0.000 out=0`

### 在线调整

```
pid l kp <val>      设定左电机 Kp
pid l ki <val>      设定左电机 Ki
pid l kd <val>      设定左电机 Kd
pid r kp <val>      右电机同理
```

| 示例 | 说明 |
|---|---|
| `pid l kp 1.5` | Kp 改为 1.5，自动复位积分 |
| `pid l ki 0.2` | Ki 改为 0.2 |
| `pid l kd 0.5` | Kd 改为 0.5 |

回显：`L Kp=1.500`。默认值：Kp=1.0, Ki=0.1, Kd=0.3。

### 文本 PID 调试

```
pid l on            开启左电机 PID 文本打印（100ms 周期）
pid off             关闭 PID 文本调试
```

输出：`[PIDL] tgt:50 cur:48 err:2 out:150`

建议用 VOFA+ 替代文本打印，波形更直观。

---

## VOFA+ 波形

```
vofa l on           开启左电机 FireWater 波形（5ms 周期）
vofa r on           开启右电机波形
vofa off            关闭 VOFA 波形
```

回显：`L VOFA ON` / `VOFA OFF`

输出 4 通道 CSV：`target_rpm,cur_rpm,pid_out,error`
详见 `docs/vofa_pid_tuning_guide.md`。

---

## 系统

```
echo <msg>          回显（串口测试）
help                显示帮助
```

---

## 错误日志

当故障发生时，通过 TX 队列自动推送错误信息，与命令回显共用同一输出流。

格式：`[ERR] <消息>`

| 触发条件 | 输出 |
|---|---|
| CAN 控制帧 200ms 超时 | `[ERR] CAN L timeout, brake` |

错误日志走队列推送链路，不与命令回显冲突。

### 推送链路

```
任意模块检测到故障
    ↓
Uart_Error("msg")     → 推入 TX 队列（非阻塞）
    ↓
Task_Uart_Tx (20ms)   → Usart_Tx_Process → 消费队列发送
```
