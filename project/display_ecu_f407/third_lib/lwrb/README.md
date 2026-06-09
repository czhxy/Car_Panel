# lwrb — Lightweight ring buffer

源码搬运自上游 `lwrb-develop` 分支（`lwrb/src/lwrb/lwrb.c`、`lwrb/src/lwrb/lwrb_ex.c`、
`lwrb/src/include/lwrb/lwrb.h`）。原始版权与许可见同目录 `LICENSE`（MIT）。

版本：v3.2.0（与上游 `lwrb-develop` HEAD 一致）。

## 集成到本工程的要点

- 头文件路径：`third_lib/lwrb/src/include`
  - 在 Keil C51/C99 工程中作为 `IncludePath` 添加。
  - 在代码里使用 `#include "lwrb/lwrb.h"`。
- 源文件：`third_lib/lwrb/src/lwrb/lwrb.c`、`lwrb_ex.c`
  - `lwrb_ex.c` 默认用 `#if defined(LWRB_DEV)` 包住 `lwrb_overwrite` / `lwrb_move`。
    本工程在 `Cads` 预定义里加了 `LWRB_DEV`，按需开关。
- 编译器：Keil ARMCC 5（V5.06 update 6）。
  - ARMCC 5 不支持 C11 `<stdatomic.h>`，所以在 `Cads` 预定义里加了
    `LWRB_DISABLE_ATOMIC`，让 lwrb 走普通读写。
  - 在 32 位 Cortex-M4 上读写 32 位 `lwrb_sz_t` 是原子的，关闭原子宏不影响正确性；
    ISR 与任务之间的同步仍由外设中断屏蔽 / FreeRTOS 临界区保证。

## Keil 工程中的具体改动（stm32f407.uvprojx）

1. 新建分组 `third_lib/lwrb`，加入 `lwrb.c`、`lwrb_ex.c`。
2. `Cads / VariousControls`：
   - `Define` 追加 `LWRB_DISABLE_ATOMIC,LWRB_DEV`
   - `IncludePath` 追加 `..\third_lib\lwrb\src\include`

## 使用示例

```c
#include "lwrb/lwrb.h"

static uint8_t rb_storage[256];
static lwrb_t  rb;

lwrb_init(&rb, rb_storage, sizeof(rb_storage));

/* 写：生产者（ISR / 任务） */
lwrb_write(&rb, src, len);

/* 读：消费者 */
uint8_t buf[64];
lwrb_sz_t n = lwrb_read(&rb, buf, sizeof(buf));
```

具体使用参考 `bsp/bsp_usart.c` 中 DMA→lwrb 的接收流程。
