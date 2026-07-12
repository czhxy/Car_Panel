# CCM 与 DMA 内存使用规范 —— Code Review 检查清单

> 适用项目：`display_ecu_f429`（STM32F429IGTx + FreeRTOS + Keil MDK/armcc）。
> 关联：本规范由 `docs/change_review_fix_step1.md` 的**修复 13**（FreeRTOS 堆移入 CCM）引出。
> 本文件面向**人类审查者与 AI code reviewer**：自包含、可 grep，用于在新增/修改代码时判断是否违反「DMA 缓冲不得落在 CCM」的约束。

---

## 1. 背景：为什么有这条规范

本工程已把 **FreeRTOS 堆（`ucHeap`，64KB）整体放入 CCM**（`0x10000000`），见 `app.sct` 的 `RW_IRAM2`。由此带来一个**硬约束**：

> **CCM（Core Coupled Memory）只能由 CPU 访问，DMA 控制器无法寻址。** 凡是 DMA 搬运的缓冲区，地址必须落在主 SRAM（`0x20000000` 段），**绝不能落在 CCM（`0x10000000` 段）**。

DMA 对 CCM 地址的「读」会得到未定义垃圾，「写」会写入黑洞（数据丢失且不报错）。后果是外设收发数据悄无声息地错乱——CAN/UART/SPI/ADC 报文内容损坏、采样值无意义，且**没有任何硬件异常**提示，极难定位。

凡是 `pvPortMalloc` 返回的内存，以及 FreeRTOS 用 `xQueueCreate` / `xStreamBufferCreate` / `xMessageBufferCreate` / `xTaskCreate` 等动态创建的对象内部缓冲，**当前都分配自 CCM 里的堆**——它们一律不得作为 DMA 目标。

---

## 2. 内存区划（修复 13 应用后）

| 区域 | 地址范围 | 大小 | 用途 | DMA 可达？ |
| --- | --- | --- | --- | --- |
| 主 SRAM（SRAM1 112KB + SRAM2 16KB） | `0x20000000`–`0x2001FFFF` | 128KB | 一般数据、外设/**DMA 缓冲**、静态全局变量 | ✅ 是 |
| CCM | `0x10000000`–`0x1000FFFF` | 64KB | **FreeRTOS 堆** `ucHeap`（任务栈、TCB、队列、信号量、互斥量、事件组、软件定时器、流/消息缓冲均从中分配） | ❌ **否** |

判别口诀：**地址以 `0x2000xxxx` 开头 → DMA 可用；以 `0x1000xxxx` 开头 → DMA 不可用。**

---

## 3. 核心规则（一句话）

> **任何会被 DMA 读写到的缓冲区，禁止从 FreeRTOS 堆（`pvPortMalloc`）分配，禁止用 `__attribute__` 放进 CCM。DMA 缓冲必须用静态数组（全局/BSS）或独立内存池，确保落在主 SRAM。**

---

## 4. Code Review 检查清单（逐条核查）

审查任一改动时，若命中以下任一情形，**判定违规，须退回修改**。

### 4.1 「DMA 配置 + heap 来源」组合（最常见违规）
当某缓冲指针 `p` 来自 `pvPortMalloc` / `p` 指向的对象来自 `xQueueCreate` 等动态创建 API，且 `p` 被传入下列任意 DMA 相关接口：

- DMA 通用：`DMA_Init`、`DMA_Cmd`、`DMA_StructInit`、`DMA_ITConfig`、`DMA_SetCurrDataCounter`、`DMA_GetCurrDataCounter`、`DMA_MemoryTargetConfig`、`DMA_DoubleBufferModeConfig`、`DMA_FlushModeCmd` 中以 `DMA_MemoryXBaseAddr` / `DMA_Memory0BaseAddr` / `DMA_Memory1BaseAddr` 字段绑定 `p`；
- 外设 DMA 使能：`USART_DMACmd`、`SPI_I2S_DMACmd`、`ADC_DMACmd`、`TIM_DMACmd`、`SDIO_DMACmd`、`I2C_DMACmd`；
- 或手写 `DMA_StreamX->M0AR = (uint32_t)p;` / `->M1AR`。

### 4.2 「CCM 段属性 + DMA」组合
任何带 `__attribute__((section(...CCM...)))` 或 `__attribute__((at(0x10000000)))` 的变量，被用作 DMA 内存地址。直接违规，不论来源。

### 4.3 流/消息缓冲误用
`xStreamBufferCreate` / `xMessageBufferCreate` / `xStreamBufferCreateStatic` 的非静态版本：其内部存储从 heap（CCM）分配，**不能**用作 DMA 的环形缓冲（即使语义上像 DMA 缓冲）。DMA 收齐后再 `xStreamBufferSend` 进缓冲是允许的（搬运由 CPU 完成）。

### 4.4 「中转指针」隐蔽违规
`p = pvPortMalloc(N); q = p + offset;` 然后 `q` 被传给 DMA —— 仍违规（派生指针仍指向 CCM）。

### 4.5 DMA 双缓冲 / 半传输回调里取地址
回调中 `xMessageBufferReceive`/直接读 DMA 缓冲：确认该缓冲是静态分配（主 SRAM），不是 heap。

---

## 5. 违规示例 ❌ 与正确示例 ✅

```c
/* ❌ 违规：DMA 缓冲从 heap 分配 —— heap 在 CCM，DMA 读到垃圾 */
void uart_rx_dma_bad(void) {
    uint8_t *rx = pvPortMalloc(256);          /* rx 落在 0x1000xxxx（CCM） */
    DMA_InitTypeDef d = {0};
    d.DMA_Memory0BaseAddr = (uint32_t)rx;     /* DMA 不可达 → 数据损坏 */
    DMA_Init(DMA1_Stream5, &d);
}

/* ✅ 正确：DMA 缓冲用静态数组 —— 落在主 SRAM，DMA 可达 */
static uint8_t dma_rx_buf[256];               /* .bss → 0x2000xxxx（主 SRAM） */
void uart_rx_dma_good(void) {
    DMA_InitTypeDef d = {0};
    d.DMA_Memory0BaseAddr = (uint32_t)dma_rx_buf;
    DMA_Init(DMA1_Stream5, &d);
}
```

```c
/* ❌ 违规：把静态数组硬钉到 CCM，再给 DMA */
__attribute__((section("CCM_RAM"))) static uint8_t buf[128];
DMA_Init(... .DMA_Memory0BaseAddr = (uint32_t)buf ...);   /* DMA 不可达 */

/* ✅ 正确：DMA 缓冲不要加 CCM 段属性，留在默认主 SRAM */
static uint8_t buf[128];                       /* 主 SRAM，DMA 可达 */
```

---

## 6. 当前工程状态（2026-07-01 核实）

- 已全工程 grep 核实：**源码中无任何 `DMA_Init` / `DMA_Cmd` / `DMA_Stream` 调用**（仅 `.map`/`.htm`/`.lst` 构建产物含 "DMA" 字样）。
- CAN（`mod_comm_can.c`、`bsp_can.c`）与 USART（`usart.c`）当前均为**中断驱动**，不涉及 DMA。
- 结论：当前把堆放 CCM **零风险**。本规范的违规风险窗口出现在**未来引入 DMA 时**。

---

## 7. 外设 DMA 能力速查（STM32F429）

| 外设 | 是否支持 DMA | 本工程启用情况 |
| --- | --- | --- |
| USART1/2/3/6, UART4/5 | ✅ 支持 | 仅 USART1，中断驱动，未用 DMA |
| SPI1/2/3 | ✅ 支持 | 未使用 |
| ADC1/2/3 | ✅ 支持 | 未使用 |
| TIM（各通道） | ✅ 支持 | 未使用 |
| SDIO | ✅ 支持 | 未使用 |
| **CAN1/CAN2** | ❌ **不支持 DMA**（F4 的 CAN 未接入 DMA 矩阵） | 中断驱动，无 DMA 计划 |

> 因此**步骤二（电机 CAN 通讯）天然不会引入 DMA**；只要电机 CAN 沿用现有 `mod_comm_can.c` 中断框架，本规范不受影响。

---

## 8. 步骤二 / 后续开发提示

- 电机 CAN：沿用中断驱动框架即可，无需考虑 DMA/CCM 冲突。
- 若后续为提升吞吐给 **USART/SPI/ADC** 加 DMA：DMA 缓冲**必须静态分配**（主 SRAM），并在 review 时按第 4 节清单核查。
- 若显示接口（如 SPI 驱屏）用 DMA 刷帧：帧缓冲必须是主 SRAM 的静态数组（或主 SRAM 专用内存池），**不要** `pvPortMalloc` 帧缓冲。

---

## 9. 附录：如何快速判断某符号是否 DMA 可达

1. 链接后查 `mdk/Listings/stm32f429.map`：符号地址以 `0x1000xxxx` → CCM（DMA 不可达）；以 `0x2000xxxx` → 主 SRAM（DMA 可达）。
2. 不确定时，把缓冲改成 `static` 全局数组并复查 map，确认其落在 `0x2000xxxx` 段。
3. 全工程扫描可疑模式：
   ```
   grep -nE "pvPortMalloc|xStreamBufferCreate|xMessageBufferCreate"  → 检查返回值是否流向 DMA
   grep -nE "DMA_(Init|Cmd)|DMA_Memory0BaseAddr|M0AR|USART_DMACmd|SPI_I2S_DMACmd|ADC_DMACmd"  → 检查其地址参数来源
   grep -nE 'section\([^)]*CCM|at\(0x10000000'  → 检查这些变量是否被 DMA 引用
   ```
