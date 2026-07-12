# 变更说明 —— display_ecu_f429 电机 CAN 通讯框架修复（步骤二）

> 生成时间：2026-07-01
> 基线：已应用 `docs/change_review_fix_step1.md` 的修复 1–13（含 CCM 堆迁移、scatter 生效）后的工程状态。
> 范围：电机 CAN 通讯框架移植后的「补全与修复」，使后续所有 CAN 通讯统一基于该框架。
> 工具链：Keil MDK-ARM（UV4）/ armcc V5.06 / SPL + FreeRTOS V11.3.0。
> MCU：STM32F429IGTx。

---

## 0. 本文档用途（给另一台电脑上的 AI 看）

电机 CAN 通讯框架已「大体移植」进来，但 TX（发送）路径是从 RT-Thread 项目抄来后未改干净，与原有 FreeRTOS/SPL 代码拼接断裂，**当前状态根本无法编译**。本文档记录把该框架修通、并统一为「应用层统一帧 `ModCanFrame`」的全部改动。

另一台电脑上的 AI 拿到本文档 + 已应用步骤一（`change_review_fix_step1.md`）的工程副本，**逐条应用第 4 节后应得到相同结果**，并通过第 5 节编译校验（0 Error / 1 Warning）。

> 提示：本项目存在透明加密，跨机器交付仅 `.md` 可读，不依赖 git/diff（见 `change_review_fix_step1.md` 第 0 节）。

---

## 1. 框架现状（修复前）

电机 CAN 框架由以下文件组成（均已加入 `mdk/app.uvprojx` 工程，参与编译）：

| 层 | 文件 | 职责 |
| --- | --- | --- |
| 协议定义 | `protocol/CAN_Protocol.h` | 29 位 ID 字段定义、mode_id 字典、编解码 inline（**当前未被任何 .c 引用**，见观察 O1） |
| 通信框架 | `task/mod_comm_can.c/.h` | 29 位 ID 宏、TX/RX 队列、ISR、收发任务、弱符号 RX 回调、`ModCanFrame` 平台无关帧 |
| 应用层 | `task/task_comm_can_protocol.c/.h` | 电机控制帧 `CanProtocol_WheelCtlSend` |
| 电机数据 | `driver/mod_motor.c/.h` | `Mod_Motor_Get_Speed/Angle` |
| 硬件 | `app/bsp_can.c/.h` | CAN1 初始化（GPIO/时钟/波特率/滤波器/NVIC）、500kbps |

RX 路径（`Mod_Can_RxIRQHandler` → `CanRxQueue` → `Mod_Can_RxTask` → 弱符号 `ModCommCan_OnRxFrame`）**结构正确、可工作**，本次不动。问题全部集中在 **TX 路径**与**未补全的实现**。

---

## 2. 问题清单（移植未完成处）

### 2.1 编译 / 链接级（不修则 build 失败）
1. `mod_comm_can.c` 首部 `#include "task_comm_can.h"` —— 该文件**不存在**（实际为 `task_comm_can_protocol.h`）。
2. `mod_comm_can.c` 中 `ModCanFrame_To_CanMsg()` 使用 **RT-Thread 残留类型** `struct rt_can_msg`、`RT_CAN_EXTID/STDID/RTR/DTR`、`rt_memcpy`，本工程（FreeRTOS+SPL）均不存在。
3. `mod_comm_can.c` 中 `ModCommCan_Tx()` 内 `vTaskDelay(pdMS_TO_TICKS(5);` —— **括号不匹配**（`pdMS_TO_TICKS(5)` 缺右括号），语法错误；且函数内 `tx_pack`/`tx_msg` 未声明。
4. **TX 链路三种类型混用**：`Mod_Can_TxEvent` 形参声明为 `CAN_FrameType`（枚举），TX 队列元素声明为 `sizeof(CanTxMsg)`，而调用方（`Mod_Can_TxTest`/`Can_Heartbeat` 传 `CanTxMsg`、`task_comm_can_protocol.c` 传 `ModCanFrame`）各不相同 → 类型全面冲突。
5. `driver/mod_motor.c` 仅有 `#include "mod_motor.h"`，`Mod_Motor_Get_Speed` / `Mod_Motor_Angle` **无定义** → 链接失败。

### 2.2 逻辑级（编译过但行为错）
6. `CanProtocol_WheelCtlSend()` 构造了 8 字节 `data`，但**从未调用发送**，末尾仅更新时间戳 → 电机控制帧空转、不发。
7. `Mod_Can_TxTask()` 中通过 `ModCommCan_Tx()` 的 `xQueueReceive(..., portMAX_DELAY)` 长期阻塞，导致同循环里的 `CanProtocol_WheelCtlSend()` 周期推送**永远执行不到**。

### 2.3 架构级（见第 6 节观察项，本次不强改）
8. `CAN_Protocol.h` 与 `mod_comm_can.h` **双重定义**协议字段（优先级/帧类型/地址），命名不一；实际代码使用 `mod_comm_can.h` 那套，`CAN_Protocol.h` 处于「加入工程但无 .c 引用」的孤立状态。

---

## 3. 统一方案

**TX 路径全部统一为 `ModCanFrame`**（平台无关帧，符合 `mod_comm_can.h` 中该结构体注释「应用层统一使用、与芯片无关」）：

```
应用层(Task/TxTest/Heartbeat)          硬件(SPL)
  ModCanFrame ──Mod_Can_TxEvent──▶ TX队列(ModCanFrame)
                                       │ Mod_Can_TxTask 取出
                                       ▼
                                  CanTxMsg ──CAN_Transmit──▶ CAN1
```

- TX 队列元素：`ModCanFrame`；`Mod_Can_TxEvent(const ModCanFrame *)` 为唯一发送入口。
- `Mod_Can_TxTask` 负责 `ModCanFrame → CanTxMsg` 转换并提交硬件邮箱。
- RX 路径保持 `CanRxMsg`（ISR 直接产生，弱符号回调基于它），不强改，降低风险。

`mode_id` 使用数值 `0x020`（即 `MODE_ID_CTRL_LF`，左前轮控制帧），以 `#define CAN_MODE_ID_CTRL_LF` 形式定义于 `task_comm_can_protocol.c`，避免 include `CAN_Protocol.h` 引发的协议宏重定义（见 O1）。

---

## 4. 已实施的修复（逐条，含改前 / 改后）

涉及文件（共 5 个）：`task/mod_comm_can.c`、`task/mod_comm_can.h`、`task/task_comm_can_protocol.c`、`task/task_comm_can_protocol.h`、`driver/mod_motor.c`。

> 缩进提醒：原 `task_comm_can_protocol.c` 部分行用 Tab；本次重写统一为空格，不影响编译。`mod_comm_can.c` 为整体重写。

### F1 —— `mod_comm_can.c`：修正错误 include `[编译]`

修改前：
```c
#include "mod_comm_can.h"
#include "task_comm_can.h"
```
修改后：
```c
#include "mod_comm_can.h"
#include "task_comm_can_protocol.h"
```

### F2 —— `mod_comm_can.c`：删除 RT-Thread 残留函数 `[编译]`

删除整个 `ModCanFrame_To_CanMsg()` 函数（使用了 `rt_can_msg`/`RT_CAN_*`/`rt_memcpy`，本工程不存在）。该函数无调用方（仅被有 bug 的 `ModCommCan_Tx` 调用，后者一并删除，见 F3）。

### F3 —— `mod_comm_can.c`：删除 buggy `ModCommCan_Tx()` `[编译/逻辑]`

删除整个 `ModCommCan_Tx()` 函数。它存在：未声明的 `tx_pack`/`tx_msg`、类型不符的 `ModCanFrame_To_CanMsg` 调用、**括号不匹配** `vTaskDelay(pdMS_TO_TICKS(5);`、以及「先转换后出队」的倒序逻辑。其职责（出队 + 硬件发送）改由重写后的 `Mod_Can_TxTask` 直接承担（F5）。

> 📌 **后续演进注记**：步骤二后期按「发送/推送分离」需求，在 **F5a 中以正确实现重新引入** 了 `ModCommCan_Tx()`（非阻塞消费 TX 队列、提交硬件），`Mod_Can_TxTask` 改为调用它。故**当前工程中 `ModCommCan_Tx` 存在且为正确版本**——本条 F3 删除的是原始 buggy 版（含 RT-Thread 残留调用与括号语法错）。

### F4 —— `mod_comm_can.c` + `mod_comm_can.h`：统一 TX 帧类型为 `ModCanFrame` `[编译/架构]`

**F4a 队列元素类型**（`Mod_Can_Init`）：

修改前：
```c
void Mod_Can_Init(void)
{
    CanTxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanTxMsg));
    CanRxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanRxMsg));
}
```
修改后：
```c
void Mod_Can_Init(void)
{
    CanTxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(ModCanFrame));
    CanRxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanRxMsg));
}
```

**F4b `Mod_Can_TxEvent` 签名与实现**（`.c` 与 `.h` 两处对应）：

修改前（`.h` 声明）：
```c
bool Mod_Can_TxEvent(CAN_FrameType tx_message);
```
修改前（`.c` 定义）：
```c
bool Mod_Can_TxEvent(CAN_FrameType tx_message)
{
    if (CanTxQueue == NULL) { return false; }
    if (xQueueSend(CanTxQueue, &tx_message, 0) == pdPASS) { return true; }
    event_err_count.tx_err_count++;
    return false;
}
```
修改后（`.h` 声明）：
```c
bool Mod_Can_TxEvent(const ModCanFrame *frame);   /* 应用层统一发送入口（平台无关帧）*/
```
修改后（`.c` 定义）：
```c
bool Mod_Can_TxEvent(const ModCanFrame *frame)
{
    if (CanTxQueue == NULL || frame == NULL) { return false; }
    if (xQueueSend(CanTxQueue, frame, 0) == pdPASS) { return true; }
    event_err_count.tx_err_count++;
    return false;
}
```

**F4c `Mod_Can_TxTest` 与 `Can_Heartbeat` 改用 `ModCanFrame`**：原函数构造 `CanTxMsg tx_msg` 后 `Mod_Can_TxEvent(tx_msg)`（传结构体给枚举形参，错误）。改为构造 `ModCanFrame frame` 并 `Mod_Can_TxEvent(&frame)`。以 `Can_Heartbeat` 为例：

修改后（节选）：
```c
void Can_Heartbeat(void)
{
    static uint32_t sHeartbeatCnt = 0;
    ModCanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.id  = CAN_HEARTBEAT_ID;
    frame.ide = MOD_CAN_IDE_EXT;
    frame.rtr = MOD_CAN_RTR_DATA;
    frame.dlc = 8;
    sHeartbeatCnt++;
    frame.data[0] = (uint8_t)(sHeartbeatCnt);
    /* ...data[1..3] 为计数高位，data[4]=状态，data[5..7] 预留... */
    Mod_Can_TxEvent(&frame);
}
```
`Mod_Can_TxTest` 同理（`CAN_TX_ID`、递增 `seq` 填 `data[0..7]`、`Mod_Can_TxEvent(&frame)`）。

### F5 —— `mod_comm_can.c`：发送/推送分离，统一消费入口 `ModCommCan_Tx` `[逻辑/架构]`

发送任务采用「① 数据发送 → ② 数据推送 → ③ 让出 CPU」结构：**发送统一封装为 `ModCommCan_Tx()`**（非阻塞消费 TX 队列、提交硬件）；**各数据帧推送各自封装为函数**（`CanProtocol_WheelCtlSend` 等），在任务循环里依次调用。**不使用 RT-Thread**，延时用 `vTaskDelay`。

**F5a 新增 `ModCommCan_Tx()`（数据发送 = 统一消费）**：非阻塞 `xQueueReceive(...,0)` 取尽当前队列，每帧 `ModCanFrame → CanTxMsg → CAN_Transmit`；邮箱满则回灌队首并 `break`（本轮结束、下一轮再试），队列空立即返回。
```c
void ModCommCan_Tx(void)
{
    ModCanFrame frame;
    while (xQueueReceive(CanTxQueue, &frame, 0) == pdPASS) {
        CanTxMsg tx_msg;
        memset(&tx_msg, 0, sizeof(tx_msg));
        if (frame.ide == MOD_CAN_IDE_EXT) { tx_msg.ExtId = frame.id & 0x1FFFFFFFU; tx_msg.IDE = CAN_ID_EXT; }
        else                               { tx_msg.StdId = frame.id & 0x7FFU;       tx_msg.IDE = CAN_ID_STD; }
        tx_msg.RTR = (frame.rtr == MOD_CAN_RTR_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        tx_msg.DLC = (frame.dlc > 8) ? 8 : frame.dlc;
        memcpy(tx_msg.Data, frame.data, tx_msg.DLC);
        if (CAN_Transmit(CAN1, &tx_msg) == CAN_TxStatus_NoMailBox) {
            xQueueSendToFront(CanTxQueue, &frame, 0);   /* 回灌，下一轮再试 */
            break;
        }
    }
}
```
（`mod_comm_can.h` 同步加 `void ModCommCan_Tx(void);` 声明。）

**F5b `Mod_Can_TxTask` 改为统一格式**（发送在前、推送在后、`vTaskDelay(1)` 收尾）：
```c
void Mod_Can_TxTask(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        ModCommCan_Tx();               /* ① 数据发送：统一消费 TX 队列 */
        // CanProtocol_HeartbeatCheck();   /* 预留 */
        CanProtocol_WheelCtlSend();    /* ② 数据推送：电机控制帧（10ms 限频）*/
        CanProtocol_WheelDebugQuery(); /* ② 数据推送：电机调试查询（占位，见 F8）*/
        vTaskDelay(pdMS_TO_TICKS(1));  /* ③ 让出 CPU（FreeRTOS，非 rt_thread_delay）*/
    }
}
```
> 说明：模板里的 `ModCommCan_Init()` 不放进任务——队列初始化必须留在 `Task_Entry_All`、且先于 `BSP_CAN_Init()` 开接收中断（步骤一修复 8），否则 `Mod_Can_RxTask` 会在 NULL 队列上 assert、首帧丢失。

### F6 —— `task_comm_can_protocol.c`：接通发送链路 + 传指针 `[逻辑/编译]`

**F6a** `CanProto_SendFrame` 末尾 `Mod_Can_TxEvent(tx)` → `Mod_Can_TxEvent(&tx)`（匹配 F4b 新签名）。

**F6b** `CanProtocol_WheelCtlSend` 构造 `data` 后**实际发送**（原代码丢弃数据），并新增 `#define CAN_MODE_ID_CTRL_LF 0x020U`：

修改前（末尾）：
```c
    data[7] = 3;
    last_send_tick = now;     /* 构造完即返回，从未发送 */
}
```
修改后（末尾）：
```c
    data[7] = 3;
    CanProto_SendFrame(CAN_PRIO_REALTIME, CAN_DEVICE_ID_MOTOR,
                       CAN_FTYPE_NORMAL, CAN_MODE_ID_CTRL_LF, 0, data, 8);
}
```
（同时把 `last_send_tick = now;` 提前到限频判断之后、构造数据之前，语义更准。）

### F7 —— `driver/mod_motor.c`：补全占位实现 `[链接]`

修改前（整文件）：
```c
#include "mod_motor.h"
```
修改后：
```c
#include "mod_motor.h"

/* TODO: 待接入真实电机编码器/传感器读取，替换占位返回。
 *   Mod_Motor_Get_Speed : 转速 rpm ;  Mod_Motor_Angle : 角度 度
 *   上层 CanProtocol_WheelCtlSend 会将值 *10 后按 int16 编码进控制帧。 */
float Mod_Motor_Get_Speed(void) { return 0.0f; }
float Mod_Motor_Angle(void)     { return 0.0f; }
```

### F8 —— `task/task_comm_can_protocol.c/.h`：新增 `CanProtocol_WheelDebugQuery` 占位 `[框架]`

为发送任务的「多推送函数」结构提供第二个推送点。当前为占位空函数（保留调用点，待定义调试查询协议后填充），避免向总线发出未定义协议帧。

`.h` 加声明：
```c
void CanProtocol_WheelCtlSend(void);
void CanProtocol_WheelDebugQuery(void);
```
`.c` 加占位实现：
```c
void CanProtocol_WheelDebugQuery(void)
{
    /* 占位：待定义调试查询协议后，在此构造帧并 CanProto_SendFrame(...) 入队 */
}
```

---

## 5. 编译验证（应用本文件后）

`UV4 -r mdk/app.uvprojx` 全量重建：
- **0 Error, 1 Warning**：唯一警告仍为 `third_lib/FreeRTOS/src/port.c(836): #550-D "ucCurrentPriority" was set but never used`（FreeRTOS 库文件本身，与本次改动无关）。
- `mod_motor.c`、`mod_comm_can.c`、`task_comm_can_protocol.c` 均成功编译；**linking 通过**（`Mod_Motor_Get_Speed/Angle`、`CanProtocol_WheelCtlSend` 等符号全部解析）。
- Program Size：Code=21200 RO-data=644 RW-data=216 ZI-data=71864（ZI 含 CCM 内 64KB 堆，见步骤一修复 13）。
- 行为不变量：RX 路径（`Mod_Can_RxIRQHandler`/`Mod_Can_RxTask`/`ModCommCan_OnRxFrame`）未改；TX 路径语义升级为「应用层统一 `ModCanFrame`」，按键测试帧（`CAN_Test_Task`/`Mod_Can_TxTest`）、心跳帧（`Can_Heartbeat`）、电机控制帧（`CanProtocol_WheelCtlSend`，10ms 周期）均经 `Mod_Can_TxEvent` → TX 队列 → `Mod_Can_TxTask` 发出。

---

## 6. 未自动修改的观察 / 建议（需人工确认）

| 编号 | 位置 | 现象 | 建议 | 未改原因 |
| --- | --- | --- | --- | --- |
| O1 | `protocol/CAN_Protocol.h` vs `task/mod_comm_can.h` | 两文件**重复定义**协议字段（优先级 `CAN_PRIO_*`、帧类型 `CAN_FTYPE_*` 同名；地址一为 `CAN_ADDR_*` 一为 `CAN_DEVICE_ID_*`）。实际代码用 `mod_comm_can.h` 那套；`CAN_Protocol.h` 无任何 .c 引用，且其 `MODE_ID_*` 字典更完整 | 后续选定一套作为唯一协议源：建议把 `CAN_Protocol.h` 的 `MODE_ID_*` 并入 `mod_comm_can.h`，删除重复定义；或反过来统一到 `CAN_Protocol.h` | 涉及全局命名/包含关系重构，超出「修通框架」范围；当前 `CAN_Protocol.h` 不参与编译，不影响构建 |
| O2 | `protocol/CAN_Protocol.h` | 中文注释为 **GBK 乱码**（在 UTF-8 环境显示乱码） | 转 UTF-8 | 自动转码有破坏风险；该文件当前未被引用 |
| O3 | `mod_comm_can.c` `ModCommCan_PrintRxFrame` | 逐字节 `LOG_I(" %02X", ...)` 打印，每字节一次调用，输出碎片化 | 拼接为一行打印 | 非功能问题，调试用；保留 |
| O4 | `driver/mod_motor.c` | `Mod_Motor_Get_Speed/Angle` 返回固定 `0.0f` | 接入真实电机传感器/编码器读取（这是步骤二的业务数据源，属下一步开发） | 框架阶段占位，待硬件/业务确定 |

---

## 7. 给「另一台电脑 AI」的应用顺序建议

1. 先确认已应用 `docs/change_review_fix_step1.md`（修复 1–13），尤其是修复 13（CCM 堆 + scatter 生效），否则 RAM 布局与本文件前提不一致。
2. `task/mod_comm_can.c`（F1–F5：建议**整体替换**为本文件对应实现，改动集中且相互依赖）。
3. `task/mod_comm_can.h`（F4b：`Mod_Can_TxEvent` 声明改为 `const ModCanFrame *`）。
4. `task/task_comm_can_protocol.c` + `.h`（F6 接通发送 / 传指针 / `CAN_MODE_ID_CTRL_LF`；F8 加 `CanProtocol_WheelDebugQuery` 占位与声明）。
5. `driver/mod_motor.c`（F7：补两个占位函数）。
6. 应用完成后按第 5 节编译校验（期望 0 Error / 1 Warning，警告为 port.c 库文件）。

> 后续所有 CAN 通讯统一遵循本框架：发送侧构造 `ModCanFrame` → `Mod_Can_TxEvent(&frame)`；接收侧在应用层重写弱符号 `ModCommCan_OnRxFrame`（默认实现为打印）。新增 DMA 时须遵守 `docs/ccm_dma_guideline.md`（堆在 CCM，DMA 缓冲禁从 heap 分配）。
