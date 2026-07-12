# 变更说明 —— display_ecu_f429 项目「检查与修复」（步骤一）

> 生成时间：2026-07-01
> 基线：本变更所基于的「当前项目状态」= 未应用本文件的原始工程（另一台电脑上的副本应与此一致）。
> 工具链：Keil MDK-ARM（UV4），编译器 armcc/armclang，SPL 标准外设库 + FreeRTOS V11.3.0。
> MCU：STM32F429IGTx。

---

## 0. 本文档用途（给另一台电脑上的 AI 看）

本文档记录对 `display_ecu_f429` 项目做的一次「完整阅读 → 排查问题 → 修复」的全部改动。
**目标**：另一台电脑上的 AI 拿到本文档 + 当前（未修改的）项目副本，**逐条应用第 2 节的修改后，应得到与源机器完全相同的结果**，并通过第 4 节的编译校验。

每条改动包含：
- **文件**：相对工程根目录的路径。
- **定位**：所在函数 / 锚点。
- **修改前 / 修改后**：原始片段与新片段（注意保持原文件的缩进字符——本项目部分文件用 Tab、部分用空格，逐字保持）。
- **类别**：`[逻辑]` `[配置]` `[质量]` `[注释]`。
- **原因**：为什么改。

> 提示：本项目存在**透明加密**——源机器上 `.c/.h/二进制` 可读，但脱离源机器（如上传 GitHub）会变乱码；**仅 `.md` 可跨机器阅读**。因此变更以本文档（`.md`）为准，不依赖 git/diff。

---

## 1. 检查范围与总体结论

### 1.1 审查范围
- **自研代码（逐行审）**：`app/`、`bootloader/`、`components/`、`driver/`、`task/`。
- **库代码（仅用于核对配置，不审逻辑）**：`firmware/`（SPL + CMSIS）、`third_lib/FreeRTOS/`。
- **检查维度**：① 逻辑/正确性（重点）；② 代码质量/风格（重点）；③ 外设/时钟/中断/Flash 布局等配置（也查）。编译/构建作为**验证手段**（非检查目标）。

### 1.2 关键配置核对（已确认正确，无需改）
- **时钟树**：HSE = 25MHz → PLL(M=25, N=360, P=2) → SYSCLK = 180MHz；HCLK = 180MHz；APB1 = HCLK/4 = **45MHz**；APB2 = HCLK/2 = 90MHz。Flash 5 WS + Over-drive 已启用（见 `system_stm32f4xx.c`）。
- **CAN 波特率**：APB1 45MHz / 分频9 / (1+BS1 7 + BS2 2) = **500kbps**，配置正确（见 `app/bsp_can.c`）。
- **NVIC 优先级分组**：`NVIC_PriorityGroup_4`（4 位抢占 / 0 位子优先级），与 FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY = (5<<4)` 配合：CAN1_RX0 抢占优先级 5（== 阈值，允许调用 FromISR API），合法。
- **工程宏**：`mdk/app.uvprojx` = `STM32F429_439xx,USE_STDPERIPH_DRIVER`；`mdk/boot.uvprojx` 追加 `BOOTLOADER`（用于条件编译：CAN1_RX0_IRQHandler、mod_comm_can 仅 App 编译）。
- **向量表**：Bootloader `VECT_TAB_OFFSET=0x00`（0x08000000）；App `VECT_TAB_OFFSET=0x00020000`（0x08020000），且 App 在 `main()` 里再次显式设置 `SCB->VTOR=&__Vectors`。

### 1.3 编译验证结论（修复后）
用 `UV4 -r` 全量重建：
| 工程 | 结果 |
| --- | --- |
| `mdk/app.uvprojx` | **0 Error, 1 Warning**（警告仅 `third_lib/FreeRTOS/src/port.c(836): #550-D "ucCurrentPriority" was set but never used`，属 FreeRTOS 库文件本身、与本次改动无关，重建前后均存在） |
| `mdk/boot.uvprojx` | **0 Error, 0 Warning** |

### 1.4 被修改的文件清单（共 13 个）
```
app/main.c
task/task_entry.c
task/mod_comm_can.c
driver/usart.c
driver/usart.h
bootloader/ymodem.c
bootloader/ota_params.c
bootloader/ota_params.h
bootloader/boot_jump.c
mdk/app.sct
mdk/bootloader.sct
mdk/app.uvprojx
mdk/boot.uvprojx
```

---

## 2. 已实施的修复（逐条）

### 修复 1 —— `app/main.c`：消除「裸 if + 缩进」陷阱 `[逻辑/质量]`
**定位**：`main()` 中创建 `Task_Entry_All` 任务处。

**修改前**：
```c
    if(xTaskCreate(Task_Entry_All, "ALL_Task_Entry", 256, NULL, 30, NULL) != pdPASS)
        LOG_E("[Main] ALL_Task_Entry create failed!\r\n");
		vTaskStartScheduler();
```

**修改后**（给 if 体补 `{}`；`vTaskStartScheduler();` 行的 Tab 缩进保持原样不动）：
```c
    if(xTaskCreate(Task_Entry_All, "ALL_Task_Entry", 256, NULL, 30, NULL) != pdPASS) {
        LOG_E("[Main] ALL_Task_Entry create failed!\r\n");
    }
		vTaskStartScheduler();
```

**原因**：原 `if` 无大括号，`vTaskStartScheduler()` 实际在 `if` 之外（总会执行），但其 Tab 缩进使其视觉上像是 `if` 体的一部分——典型「裸 if」陷阱。补 `{}` 明确语义；**行为不变**（无论任务是否创建成功都启动调度器）。

---

### 修复 2 —— `bootloader/ymodem.c`：进度打印除零保护 `[逻辑]`
**定位**：`ymodem_receive()` 数据包接收循环中的进度打印块（`status->packet_count % 16 == 0` 分支）。

**修改前**：
```c
        if (status->packet_count % 16 == 0) {
            printf("[YMODEM] %u/%u (%u%%)\r\n",
                   (unsigned int)status->total_received,
                   (unsigned int)status->file_size,
                   (unsigned int)(status->total_received * 100 /
                                  status->file_size));
        }
```

**修改后**：
```c
        if (status->packet_count % 16 == 0) {
            /* file_size 可能为 0（文件名包未携带大小），需防除零 */
            if (status->file_size > 0) {
                printf("[YMODEM] %u/%u (%u%%)\r\n",
                       (unsigned int)status->total_received,
                       (unsigned int)status->file_size,
                       (unsigned int)(status->total_received * 100 /
                                      status->file_size));
            } else {
                printf("[YMODEM] %u/%u (size unknown)\r\n",
                       (unsigned int)status->total_received,
                       (unsigned int)status->file_size);
            }
        }
```

**原因**：`parse_filename_packet()` 在文件名包未携带大小时会让 `status->file_size` 保持 0；整数除以 0 会触发 `UsageFault(DIVBYZERO)` → HardFault。加 `file_size > 0` 判断。

---

### 修复 3 —— `driver/usart.c`：`UART_Printf` 改用 `vsnprintf` `[逻辑/安全]`
**定位**：`UART_Printf()` 内。

**修改前**：
```c
	vsprintf(String, format, arg);
```
**修改后**：
```c
	vsnprintf(String, sizeof(String), format, arg);
```
**原因**：原 `vsprintf` 写入 200 字节栈缓冲 `char String[200]`，无长度限制；格式串产出 >200 字节即栈溢出。改 `vsnprintf` 限定上界。（注意该行行首是 1 个 Tab，逐字保持。）

---

### 修复 4 —— `driver/usart.c`：发送循环索引类型放大 `[逻辑]`
**定位**：`UART_SendString()` 与 `UART_SendNumber()` 中的循环变量声明。

**修改前**（两处均为）：
```c
	uint8_t i;
```
**修改后**（两处均为）：
```c
	size_t i;
```
**原因**：`UART_SendString()` 用 `uint8_t i` 遍历字符串，字符串 >255 字节时 `i` 回绕 → 永不命中 `'\0'` → 越界/死循环。改 `size_t`。`UART_SendNumber()` 同改（其 `Length` 为 `uint8_t`，改后无副作用）。

> 实施方式：对 `uint8_t i;` 做 `replace_all`（`usart.c` 中仅这两处出现）。

---

### 修复 5 —— `driver/usart.c`：USART1 NVIC 子优先级非法值 `[配置]`
**定位**：`UART_Init()` 的 NVIC 配置。

**修改前**：
```c
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 6;
```
**修改后**：
```c
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0; /* Group_4 下子优先级无位，必须为 0 */
```
**原因**：`main()` 设置 `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4)` → 4 位抢占 / **0 位子优先级**。子优先级必须为 0；原值 6 无效（被硬件掩码为 0，属配置错误/误导）。（`bsp_can.c` 中 CAN 的 `SubPriority=0` 本就正确。）

---

### 修复 6 —— `driver/usart.c` + `driver/usart.h`：只读参数加 `const` `[质量]`
**定位**：`UART_SendArray` / `UART_SendString` 的声明与定义（`.c` 与 `.h` 两处对应）。

**修改前**：
```c
void UART_SendArray(uint8_t *Array, uint16_t Length)     // usart.c
void UART_SendString(char *String)                       // usart.c
void UART_SendArray(uint8_t *Array, uint16_t Length);    // usart.h
void UART_SendString(char *String);                      // usart.h
```
**修改后**：
```c
void UART_SendArray(const uint8_t *Array, uint16_t Length)     // usart.c
void UART_SendString(const char *String)                       // usart.c
void UART_SendArray(const uint8_t *Array, uint16_t Length);    // usart.h
void UART_SendString(const char *String);                      // usart.h
```
**原因**：这两个函数不修改入参缓冲，加 `const` 表意更准、便于传只读数据。调用方（如 `task_query.c` 传 `uint8_t buf[32]`、`UART_Printf` 传 `char String[200]`）无需改动。

---

### 修复 7 —— `task/mod_comm_can.c` + `.h`：落实 `can_rx_isr_cnt` 诊断计数 `[逻辑/质量]`
**定位**：`Mod_Can_RxIRQHandler()`（ISR）与文件顶部变量区。

**问题**：`mod_comm_can.h` 声明 `extern volatile uint32_t can_rx_isr_cnt;` 并注释「在 ISR 中递增」，但全工程**既无定义、ISR 中也未递增**——属悬挂符号 + 误导注释。

**修改 A（`mod_comm_can.c`，在 `ModCan_RxErrCount` 定义之后新增定义）**：

修改前：
```c
/* ---- 统计变量（非静态，供外部只读访问） ---- */
const uint8_t *ModCan_TxErrCount  = &event_err_count.tx_err_count;
const uint8_t *ModCan_RxErrCount  = &event_err_count.rx_err_count;
```
修改后：
```c
/* ---- 统计变量（非静态，供外部只读访问） ---- */
const uint8_t *ModCan_TxErrCount  = &event_err_count.tx_err_count;
const uint8_t *ModCan_RxErrCount  = &event_err_count.rx_err_count;

/* ---- 诊断计数器：CAN RX 中断帧数（定义于此处，供外部诊断读取） ---- */
volatile uint32_t can_rx_isr_cnt = 0;
```

**修改 B（`mod_comm_can.c`，`Mod_Can_RxIRQHandler()` 的 while 循环内，每收一帧递增）**：

修改前：
```c
    while (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
        CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);

        if (CanRxQueue != NULL) {
            xQueueSendFromISR(CanRxQueue, &rx_msg, &xHigherPriorityTaskWoken);
        }

    }
```
修改后：
```c
    while (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
        CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);
        can_rx_isr_cnt++;   /* 累计 RX 中断帧数，供诊断 */

        if (CanRxQueue != NULL) {
            xQueueSendFromISR(CanRxQueue, &rx_msg, &xHigherPriorityTaskWoken);
        }

    }
```

**原因**：兑现头文件「CAN RX 帧数，在 ISR 中递增」的约定，提供真实诊断计数（对后续电机 CAN 调试有用），并消除悬挂 `extern`。`mod_comm_can.h` 中声明保持不变。

---

### 修复 8 —— `task/task_entry.c`：CAN 队列与中断的初始化顺序 `[逻辑]`
**定位**：`Task_Entry_All()` 开头的初始化序列。

**修改前**：
```c
    BSP_LED_Init();
    BSP_KEY_Init();
		BSP_CAN_Init();
    /* 提前创建 CAN 队列，避免 RX 任务在队列就绪前启动 */
    Mod_Can_Init();
```
**修改后**：
```c
    BSP_LED_Init();
    BSP_KEY_Init();
    /* 先创建 CAN 收发队列，再初始化 CAN 硬件并使能接收中断，
     * 确保接收中断触发时队列已就绪（避免首帧丢失） */
    Mod_Can_Init();
    BSP_CAN_Init();
```
**原因**：`BSP_CAN_Init()` 会使能 CAN1 FIFO0 接收中断（`CAN_IT_FMP0` + NVIC）；原顺序下，中断在「使能」与「`Mod_Can_Init()` 创建队列」之间的窗口内触发时，因 `CanRxQueue==NULL`（ISR 内有 NULL 检查，不致崩溃）会**丢弃首帧**。改为先建队列再开中断，与代码注释意图一致。（`BSP_CAN_Init()` 那行原本是 2 个 Tab 缩进，调整后改为 4 空格以与上下文一致。）

---

### 修复 9 —— `task/mod_comm_can.c`：过时注释更正 `[注释]`
**定位**：`Mod_Can_TxTask` 的函数注释。

**修改前**：
```c
 * 开头调用 BSP_CAN_Init 进行硬件初始化（需在调度器启动后）
```
**修改后**：
```c
 * 从 TX 队列取帧并提交 CAN 发送邮箱；CAN 硬件初始化由 Task_Entry_All 完成
```
**原因**：`Mod_Can_TxTask` 实际并不调用 `BSP_CAN_Init`（硬件初始化在 `Task_Entry_All` 中完成），原注释过时/错误。

> 📌 **后续演进注记**：本条改动的 `Mod_Can_TxTask` 注释，在步骤二（`docs/change_motor_can_step2.md` F5）随发送任务重构（「发送/推送分离」、引入 `ModCommCan_Tx`）**再次更新**。当前工程中该函数注释以步骤二为准（「① 数据发送 → ② 数据推送 → ③ 让出 CPU」结构说明）。本条作为步骤一的历史改动记录保留。

---

### 修复 10 —— `bootloader/ota_params.c` + `ota_params.h`：CRC 算法注释更正 `[注释]`
**定位**：文件头/类型注释里的算法说明。

**修改前**：
- `ota_params.c` 文件头：`*          CRC-32/MPEG-2: 多项式 0xEDB88320（反射），查表法`
- `ota_params.h`：`// 标准 CRC-32/MPEG-2（多项式 0xEDB88320，反射）`

**修改后**：
- `ota_params.c` 文件头：`*          标准 CRC-32 (IEEE 802.3 / PKZIP): 多项式 0xEDB88320（反射），查表法`
- `ota_params.h`：`// 标准 CRC-32 / IEEE 802.3（多项式 0xEDB88320，反射）`

**原因**：实现使用「反射多项式 0xEDB88320 + 初始/异或 0xFFFFFFFF」，测试向量 `CRC32("123456789")=0xCBF43926` —— 这是**标准 CRC-32 (IEEE 802.3 / PKZIP)**，而非 CRC-32/MPEG-2（MPEG-2 不可反射、向量为 `0x0376E6E7`）。**实现本身正确**（`crc32_calc` 与 `crc32_flash` 同源），仅注释措辞误导。

---

### 修复 11 —— `mdk/app.sct` + `mdk/bootloader.sct`：SRAM 区域大小更正 `[配置]`
**定位**：散列文件中的 `RW_IRAM1` 行。

**修改前**（两个文件相同）：
```
  RW_IRAM1 0x20000000 0x00030000  {  ; 192KB SRAM
```
**修改后**（两个文件相同）：
```
  RW_IRAM1 0x20000000 0x00020000  {  ; 128KB SRAM (SRAM1 112KB + SRAM2 16KB); CCM 64KB @0x10000000 未使用
```
**原因（详解）**：

**(1) F429 的真实物理内存并不是一个连续大块。** 它分两段、地址不连续：

```
地址范围                  大小      名称      说明
──────────────────────────────────────────────────────────
0x20000000 ─┐
            │ 112 KB   SRAM1   ┐
0x2001BFFF ─┘                  │ 这两段地址相邻
0x2001C000 ─┐                  │ 合计 128KB 连续 SRAM
            │  16 KB   SRAM2   ┘
0x2001FFFF ─┘ ←── 连续 SRAM 到此为止（128KB）

0x20020000 ~ 0x2002FFFF        ╳ 64KB「黑洞」：总线未挂任何 RAM

0x10000000 ─┐
            │  64 KB   CCM     ←── 另一段，地址完全不连续；
0x1000FFFF ─┘                     只有 CPU 能访问，DMA 进不来
```

即：**从 `0x20000000` 起，连续可用的 RAM 只有 128KB**；那 64KB CCM 在 `0x10000000`，中间隔着一大片空洞，接不上。

**(2) `RW_IRAM1` 的语义 = 给链接器的一道「RAM 防溢出保险丝」。** 散列文件的 `RW_IRAM1 <起始> <大小>` 告诉链接器：把所有需要 RAM 的东西（已初始化全局变量 RW、清零变量 ZI、栈、堆、FreeRTOS 各任务栈……）放进这个区间。链接器只做一件检查：**实际需求总量是否 ≤ 声明大小**。超过 → 编译报 `Region RW_IRAM1 overflowed`，编译期拦住；没超过 → 静默通过，**不管声明的区间里有没有真实硬件**。

**(3) 原 `0x00030000`（=192KB）是错的，且错得很典型。** `0x30000 = 196608 B = 192KB = 128KB 连续 SRAM + 64KB CCM`。几乎可以肯定是作者把 CCM 也算进来整体塞进 `RW_IRAM1`。但 CCM 地址是 `0x10000000`，与 `0x20000000` 段不连续，**不能当作同一段区域**。写成 192KB 等于告诉链接器「`0x20000000`～`0x2002FFFF` 都随便用」，而 `0x20020000`～`0x2002FFFF` 这 64KB 硬件上不存在。

**(4) 为什么「现在能跑」——这正是它最坑的地方。** map 显示 App 实际只占用 `0x11988`（≈70KB），全部落在真实存在的 128KB 内，离黑洞还远，所以板子一切正常，任何功能测试都发现不了。但链接器的保险丝已被悄悄拆掉：它以为有 192KB，**永远不会报 overflow**。

**(5) 隐患何时炸。** 一旦代码/变量增长，RAM 占用跨过 `0x20020000`（128KB）那条线，链接器**仍不报错**（以为有 192KB），却把变量/栈分配到 `0x20020000` 以上的不存在内存。运行时访问 → AHB 总线无应答 → **BusFault → HardFault**；而且是间歇性的（只有执行流碰到那个变量、或栈长到那高度时才崩），现象像野指针/栈溢出/电源问题，极难定位；编译器、链接器却全绿。把「运行时随机 HardFault」提前变成「编译时一行报错」，正是链接器该干的活。

**(6) 改回 `0x00020000`（128KB）后：** ① 保险丝恢复——未来 RAM 逼近 128KB 时编译期即报警；② 当前不受影响——实际才用 70KB，远小于 128KB（已验证 app 0E/1W、boot 0E/0W）；③ CCM 的处置见下方延伸小节。

**安全性已核实**：map 显示 `__initial_sp = 0x20011988`（栈顶由 `STACK` 段落在已用 ZI 末尾，而非区域声明的顶部），远在 `0x20020000` 以内；缩小声明区域不会移动栈顶，当前构建不受影响。

> **延伸：那 64KB CCM 用在哪里、怎么正确使用**
>
> CCM = Core Coupled Memory（紧耦合内存），`0x10000000`，64KB，通过 D-bus 直连 Cortex-M4 内核，**0 等待、带宽最高、延迟最低**，上电即可用。
>
> **适合放 CCM 的东西**（都是「只由 CPU 读写、追求速度」的）：
> - FreeRTOS 的 heap（把 `configADJUSTED_HEAP_SIZE` 对应的堆放进 CCM，可把主 SRAM 让给任务栈/缓冲——这是 F4 系列 FreeRTOS 工程最常见的用法）；
> - 频繁访问的核心控制数据：PID 状态、滤波器历史、控制环中间数组；
> - 高速中断里反复读写的数据结构；
> - 大块只读查找表：FIR/CRC/三角函数表等。
>
> **绝对不能放 CCM 的东西**：
> - **任何 DMA 目标缓冲**——DMA 不连 CCM，会读到垃圾或写入黑洞。本工程的 CAN 接收是中断驱动（`Mod_Can_RxIRQHandler`），目前不涉及 DMA；但若以后给 USART/CAN/ADC 上 DMA，其 RX/TX 缓冲**严禁**落进 CCM；
> - 期望被其他总线主（调试器、其它内核）访问的数据。
>
> **正确启用方式**（本次未做，仅说明）：在散列文件**另起一段**，而不是并进主 SRAM：
> ```
>   RW_IRAM2 0x10000000 0x00010000  {  ; 64KB CCM，仅 CPU 访问，禁作 DMA 目标
> ```
> 然后用 `__attribute__((section("...")))` 或链接器符号把选定的变量/堆指向该段。本次仅把主 SRAM 区域修正为正确的 128KB，**未启用 CCM**（保持原行为），如需启用请单独评估 DMA 路径。

---

### 修复 12 —— `bootloader/boot_jump.c`：App 栈指针合法性上界收紧 `[配置]`
**定位**：`jump_to_app()` 与 `partition_is_valid()` 中的 SP 范围校验，以及文件头/行内注释（共 3 处）。

**修改 A（文件头注释）**：

修改前：
```c
 *          STM32F429: SRAM 192KB (0x20000000 - 0x2002FFFF)
```
修改后：
```c
 *          STM32F429: 连续 SRAM 128KB (0x20000000 - 0x2001FFFF); CCM 64KB @0x10000000
```

**修改 B（`jump_to_app()` 内的校验）**：

修改前：
```c
    // F429 有 192KB SRAM (0x20000000 - 0x20030000)
    if (sp < 0x20000000 || sp >= 0x20030000) {
```
修改后：
```c
    // F429 连续 SRAM 128KB (0x20000000 - 0x2001FFFF); CCM 64KB @0x10000000 不在连续区
    if (sp < 0x20000000 || sp >= 0x20020000) {
```

**修改 C（`partition_is_valid()` 内的校验）**：

修改前：
```c
    // F429: 192KB SRAM @ 0x20000000
    return (sp >= 0x20000000 && sp < 0x20030000);
```
修改后：
```c
    // F429: 连续 SRAM 128KB @ 0x20000000 (0x20000000 - 0x2001FFFF)
    return (sp >= 0x20000000 && sp < 0x20020000);
```

**原因**：与修复 11 同源。原上界 `0x20030000` 基于「192KB 连续 SRAM」的错误假设，会放行指向 `0x20020000–0x2002FFFF`（不存在内存）的 SP；收紧为 `0x20020000`。当前 App 的 `__initial_sp=0x20011988` 仍在合法范围内，跳转/校验行为不受影响。

> ⚠️ **重要更正（见修复 13）**：本条与修复 11 当时均以为改 `.sct` 即生效。后续实施修复 13 时发现两工程 `umfTarg=1`，`.sct` 被 Target Dialog 覆盖而**一直未生效**（map 中 `RW_IRAM1 Max` 仍是 `0x00030000`）。修复 13 已纠正 `umfTarg`，本条与修复 11 的 SP 上界（`0x20020000`）方才真正具备拦截意义。若只应用修复 11/12 而不动 `umfTarg`，校验代码虽编译进去，但 RAM 上限仍按 192KB 链接——请务必一并应用修复 13。

---

### 修复 13 —— 散列文件真正生效 + FreeRTOS 堆移入 CCM `[配置/性能]`

**背景与根因发现**：实施本条时发现一个**根因性问题**——两工程的 `umfTarg=1`（`<LDads>` 段中 `<umfTarg>1</umfTarg>`，意为「Use Memory Layout from Target Dialog」），使 Keil 用 Target Dialog 的内存设置**自动生成 scatter 并覆盖** `.sct` 文件。因此**修复 11 对 `app.sct`/`bootloader.sct` 的改动此前并未生效**，真实内存布局一直由 Target Dialog 控制（其 IRAM 仍为错误的 `0x30000`=192KB）。本修复一并纠正：让 `.sct` 真正接管；并在 `app.sct` 增加 CCM 区域，把 FreeRTOS 64KB 堆移入 CCM（释放主 SRAM、加速内核访问）。

> 前置评估（已核实）：FreeRTOS 堆 `configTOTAL_HEAP_SIZE = 64*1024`，实现为 `heap_4.c`（纯 CPU 链表管理、无 DMA）；CCM 64KB @ `0x10000000`，大小与堆吻合；全工程源码无任何 `DMA_Init/DMA_Cmd` 调用（仅构建产物中有），故堆入 CCM 当前零风险。新增 DMA 须遵守 `docs/ccm_dma_guideline.md`。

涉及 3 个文件、两组改动（A 让 scatter 生效；B 加 CCM 放堆）。

#### 改动组 A：让散列文件真正生效（`mdk/app.uvprojx` + `mdk/boot.uvprojx`，两文件完全对称）

**A.1 启用自定义 scatter（关闭 Target Dialog 覆盖）**

修改前：
```xml
            <umfTarg>1</umfTarg>
```
修改后：
```xml
            <umfTarg>0</umfTarg>
```
> 改为 0 后 Keil 使用 `<ScatterFile>` 指定的 `app.sct`/`bootloader.sct`，Target Dialog 的 IRAM/IROM 表不再覆盖。

**A.2 同步修正 `<Cpu>` 行的 IRAM 描述（防御性，防将来切回对话框模式再踩 192KB 坑）**

修改前（仅 IRAM 字段）：
```
IRAM(0x20000000,0x00030000) IROM(...)
```
修改后：
```
IRAM(0x20000000,0x00020000) IROM(...)
```
> `app.uvprojx` 的 IROM 为 `IROM(0x08020000,0x00060000)`；`boot.uvprojx` 为 `IROM(0x08000000,0x00010000)`，均不动，仅替换 IRAM 段。

**A.3 同步修正 OnChipMemories 的 `<IRAM>` 与 `<OCR_RVCT9>` 尺寸**

修改前（两文件各有 2 处）：
```xml
                <Size>0x30000</Size>
```
修改后：
```xml
                <Size>0x20000</Size>
```
> 可用 replace_all：文件内 `<Size>0x30000</Size>` 恰好只有 `<IRAM>` 与 `<OCR_RVCT9>` 这 2 处（其余 `<Size>` 为 `0x0`/`0x60000`/`0x10000`）。

#### 改动组 B：`mdk/app.sct` 增加 CCM 区域放置 FreeRTOS 堆（仅 App；Bootloader 不编译 FreeRTOS，`bootloader.sct` 不动）

修改前（即修复 11 应用后的状态）：
```
  RW_IRAM1 0x20000000 0x00020000  {  ; 128KB SRAM (SRAM1 112KB + SRAM2 16KB); CCM 64KB @0x10000000 未使用
   .ANY (+RW +ZI)
  }
}
```
修改后：
```
  RW_IRAM1 0x20000000 0x00020000  {  ; 128KB 主 SRAM (SRAM1 112KB + SRAM2 16KB)；外设/DMA 缓冲与一般数据
   .ANY (+RW +ZI)
  }
  ; ---- CCM 64KB @0x10000000：仅 CPU 可访问，DMA 不可达 ----
  ; ---- 放 FreeRTOS 堆 (heap_4 的 ucHeap 64KB)，释放主 SRAM；DMA 缓冲严禁经 heap 分配至此 ----
  RW_IRAM2 0x10000000 0x00010000  {
   heap_4.o (+ZI)
  }
}
```

**原因 / 收益**：
- `ucHeap`(64KB) 整体落入 CCM `0x10000000`，主 SRAM 占用从 `0x11988`(≈70KB) 骤降至 `0x1988`(≈6.4KB)，**腾出整 64KB 主 SRAM** 给后续显示/CAN/协议缓冲（对步骤二电机 CAN 及显示功能尤其宝贵）。
- CCM 0 等待，FreeRTOS 内核链表与从堆分配的任务栈访问提速。
- heap_4 纯 CPU 操作、无 DMA，放 CCM 安全；本工程当前无 DMA，零风险。

**编译验证（map 实测）**：
- **app**：`ucHeap 0x10000000 Data 65536 heap_4.o(.bss)` ✓；`Execution Region RW_IRAM2 (Exec base 0x10000000, Size 0x00010000, Max 0x00010000)` ✓；`Execution Region RW_IRAM1 (Size 0x00001988, Max 0x00020000)` ✓（主 SRAM 仅 6.4KB / 上限正确 128KB）；`__initial_sp = 0x20001988` ✓；**0 Error, 1 Warning**（同修复前，port.c 库警告）。
- **boot**：`Execution Region RW_IRAM1 (Size 0x000014a0, Max 0x00020000)` ✓（128KB 正确生效），无 RW_IRAM2、无 ucHeap；**0 Error, 0 Warning**。

---

## 3. 未自动修改的观察 / 建议（需人工确认）

以下项**未在本次改动中修改**，原因多为「依赖硬件」「改动较大有风险」或「需确认意图」。列出供你判断。

| 编号 | 位置 | 现象 | 建议 | 未改原因 |
| --- | --- | --- | --- | --- |
| R1 | `system_stm32f4xx.c` / `stm32f4xx.h` | `HSE_VALUE=25MHz`（SPL 默认），PLL 据此配 180MHz | **请核对板载晶振是否确为 25MHz**。若是 8MHz，则 PLL 输入 8/25=0.32MHz（主 PLL 推荐 1–2MHz）可能锁定不稳，SYSCLK 偏离 180MHz，CAN/UART 波特率全错 | 依赖硬件，无法仅凭代码判定；改错会让全板时钟失效 |
| R2 | `driver/Delay.c` | 该文件为 **GBK 编码**（中文注释在本环境显示为乱码），其余文件为 UTF-8；编码不统一 | 统一为 UTF-8 | 自动转码有破坏注释/被加密系统影响的风险；且 `Delay.c` 逻辑无需改（见 R3） |
| R3 | `driver/Delay.c` | `Delay_us/ms` 直接重配 SysTick；而 App 中 SysTick 归 FreeRTOS 所有 | App 侧不要调用 `Delay_*`（已确认 `app/`、`task/` 均未调用，仅 bootloader 用，安全）。如需 App 内阻塞延时请用 `vTaskDelay` | 仅作约定，无需改代码 |
| R4 | `task/task_query.c` | 重复定义了 `ota_param_snap_t`（与 `bootloader/ota_params.h` 的 `ota_param_t` 重复） | 理想做法是复用 `ota_params.h` 的类型，避免两处不同步 | App 引入 bootloader 头需评估包含路径/依赖；属可维护性建议 |
| R5 | `app/bsp_key.c` | `prvKeyScanTask` 在按键**释放**沿（RESET→SET，配合上拉+按下接地）给信号量 | 确认是否符合预期（常见也有按**按下**触发） | 行为可能是有意；改了会改变人机交互 |
| R6 | `bootloader/boot_decision.c` | OTA「启动尝试次数回滚」机制中，App 侧未见把 `ota_state` 复位/确认启动成功的动作 | 若需严格的「启动失败自动回滚」，App 启动成功后应写 OTA 参数确认（如把 `ota_state` 置 IDLE、清 `boot_count`） | 属功能设计增强，超出「修 bug」范围 |
| R7 | `mdk/app.uvprojx` / `boot.uvprojx` | 两个工程的输出都写到 `mdk/Objects/stm32f429.axf`（同名同目录），先后构建会互相覆盖 | 建议两个工程使用各自独立的输出目录（如 `mdk/Objects_app/`、`mdk/Objects_boot/`） | 改 Keil 工程输出路径较繁琐且需你验证下载配置 |
| R8 | `app/bsp_can.c` | 使能了 `RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ...)` | STM32F4 的 GPIO AF 配置并不依赖 SYSCFG 时钟（F4 不像 F7/F42x 部分场景），此处使能属多余但**无害** | 无害，保留 |

---

## 4. 复现校验方法（应用完本文件后）

1. 用 Keil 打开 `mdk/app.uvprojx`，`Project → Rebuild all target files`（或命令行 `UV4 -r mdk/app.uvprojx -o build_app.log`）。
   - 期望：**0 Error, 1 Warning**（且该警告为 `third_lib/FreeRTOS/src/port.c(836): #550-D "ucCurrentPriority" was set but never used`）。
2. 用 Keil 打开 `mdk/boot.uvprojx`，Rebuild（或 `UV4 -r mdk/boot.uvprojx -o build_boot.log`）。
   - 期望：**0 Error, 0 Warning**。
3. 关键不变量（在 `mdk/Listings/stm32f429.map` 中核对；以下为应用全部修复 1–13 后的实测值。注意 app 与 boot 的 map 同名会互相覆盖，需分别编译分别核对）：
   - **app**：`ucHeap` 落在 `0x10000000`（CCM）；`__initial_sp = 0x20001988`（主 SRAM 内）；`Execution Region RW_IRAM1 (Size 0x00001988, Max 0x00020000)`；存在 `Execution Region RW_IRAM2 (0x10000000, Size 0x00010000)`。
   - **boot**：`Execution Region RW_IRAM1 (Size 0x000014a0, Max 0x00020000)`；无 RW_IRAM2、无 ucHeap。
4. 行为不变量：本次改动**不改变任何对外功能行为**（CAN 收发、OTA/YMODEM、按键、串口协议、心跳等逻辑均与改前等价），仅修复潜在崩溃点（修复 2/3/4）、配置正确性（修复 5/11/12）、初始化顺序（修复 8）与代码/注释质量（修复 1/6/7/9/10）。

---

## 5. 给「另一台电脑 AI」的应用顺序建议

按文件分组应用可减少往返（同文件的多次编辑按本文档给出的「修改前/后」逐条替换即可）：
1. `driver/usart.c`（修复 3、4、5、6）、`driver/usart.h`（修复 6）
2. `app/main.c`（修复 1）
3. `task/task_entry.c`（修复 8）、`task/mod_comm_can.c`（修复 7、9）
4. `bootloader/ymodem.c`（修复 2）、`bootloader/ota_params.c` + `.h`（修复 10）、`bootloader/boot_jump.c`（修复 12）
5. `mdk/app.uvprojx` + `mdk/boot.uvprojx`（**修复 13 改动组 A**：`umfTarg` 1→0、`<Cpu>` IRAM、`<IRAM>`/`<OCR_RVCT9>` Size）。**此步是修复 11/12 真正生效的前提，务必先做。**
6. `mdk/app.sct`（修复 11 基础上 + **修复 13 改动组 B**：CCM 区域）、`mdk/bootloader.sct`（修复 11）。
7. 应用完成后按第 4 节编译校验（app、boot 分别编译分别核对 map）。

> 缩进提醒：本项目 `driver/usart.c`、`app/main.c`、`task/task_entry.c`、`task/mod_comm_can.c` 部分行使用 **Tab**；`bootloader/*`、散列文件、头文件多用空格。替换时请逐字保持原文缩进，避免无关 diff。
