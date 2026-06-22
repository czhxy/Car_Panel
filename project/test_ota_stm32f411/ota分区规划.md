# OTA 分区规划与实现指南 — test_ota_stm32f411

> 文档版本：v1.0
> MCU：STM32F411RET6（Flash 512KB, RAM 128KB）
> 协议：YMODEM-1K（USART1, 115200-8-N-1）
> 本指南按实现阶段编写，每个阶段末尾标注**检查点**和**常见错误**，方便定位问题。

---

## 目录

1. [分区规划](#一分区规划)
2. [阶段一：工程配置](#二阶段一工程配置)
3. [阶段二：Bootloader 骨架](#三阶段二bootloader-骨架)
4. [阶段三：Flash 操作层](#四阶段三flash-操作层)
5. [阶段四：CRC32 校验](#五阶段四crc32-校验)
6. [阶段五：OTA Parameter 管理](#六阶段五ota-parameter-管理)
7. [阶段六：YMODEM 协议实现](#七阶段六ymodem-协议实现)
8. [阶段七：升级状态机与回滚](#八阶段七升级状态机与回滚)
9. [阶段八：App 侧工程配置](#九阶段八app-侧工程配置)
10. [阶段九：PC 端发送工具](#十阶段九pc-端发送工具)
11. [阶段十：联调与验证](#十一阶段十联调与验证)
12. [附录：Flash Sector 编号表](#附录-aflash-sector-编号表fan)
13. [附录：常见 HardFault 排查](#附录-b常见-hardfault-排查)

---

## 一、分区规划

### 1.1 STM32F411RET6 Flash 扇区布局

| 扇区 | 地址范围 | 大小 | 扇区编号 (STM32F4xx_Flash_Program) |
|------|----------|------|-------------------------------------|
| Sector 0 | 0x0800 0000 – 0x0800 3FFF | 16 KB | `FLASH_Sector_0` (0) |
| Sector 1 | 0x0800 4000 – 0x0800 7FFF | 16 KB | `FLASH_Sector_1` (1) |
| Sector 2 | 0x0800 8000 – 0x0800 BFFF | 16 KB | `FLASH_Sector_2` (2) |
| Sector 3 | 0x0800 C000 – 0x0800 FFFF | 16 KB | `FLASH_Sector_3` (3) |
| Sector 4 | 0x0801 0000 – 0x0801 FFFF | 64 KB | `FLASH_Sector_4` (4) |
| Sector 5 | 0x0802 0000 – 0x0803 FFFF | 128 KB | `FLASH_Sector_5` (5) |
| Sector 6 | 0x0804 0000 – 0x0805 FFFF | 128 KB | `FLASH_Sector_6` (6) |
| Sector 7 | 0x0806 0000 – 0x0807 FFFF | 128 KB | `FLASH_Sector_7` (7) |
| **总计** | **0x0800 0000 – 0x0807 FFFF** | **512 KB** | |

### 1.2 分区方案

```
Flash 分区总览 (512KB)
╔═══════════════════════════════════════════════════════════════╗
║ 分区          │ 起始地址     │ 大小    │ 扇区       │ 说明     ║
╠═══════════════════════════════════════════════════════════════╣
║ Bootloader    │ 0x0800 0000  │ 48KB   │ Sector 0-2 │ 引导程序  ║
║ OTA Parameter │ 0x0800 C000  │ 16KB   │ Sector 3   │ OTA标志   ║
║ App A         │ 0x0801 0000  │ 192KB  │ Sector 4-5 │ 主应用    ║
║ App B         │ 0x0804 0000  │ 256KB  │ Sector 6-7 │ 备份应用   ║
╚═══════════════════════════════════════════════════════════════╝
```

地址图示：

```
0x08000000 ┌───────────────────────┐
           │     Bootloader         │ 48KB (Sector 0-2)
           │     (0x0800 0000)      │
0x0800C000 ├───────────────────────┤
           │   OTA Parameter        │ 16KB (Sector 3)
           │   (0x0800 C000)        │
0x08010000 ├───────────────────────┤
           │                       │
           │     App A              │ 192KB (Sector 4: 64KB + Sector 5: 128KB)
           │   (0x0801 0000)        │
           │                       │
0x08040000 ├───────────────────────┤
           │                       │
           │     App B              │ 256KB (Sector 6: 128KB + Sector 7: 128KB)
           │   (0x0804 0000)        │
           │                       │
0x08080000 └───────────────────────┘
```

### 1.3 设计要点

1. **Bootloader = 48KB（3×16KB）**：对齐扇区边界（必须），实际代码预计 16–24KB。
2. **Parameter = 16KB 独立扇区**：单独擦写，不影响代码区。
3. **App A = 192KB ≠ App B = 256KB**：扇区大小不均导致。App 编译约束 ≤ 192KB。
4. **所有分区必须严格扇区对齐**：STM32F4 Flash 擦除以 Sector 为单位，非扇区对齐的擦写会破坏相邻区域。

---

## 二、阶段一：工程配置

> **目标**：搭建 Bootloader 工程的编译环境，生成一个能独立运行并串口打印的 Bootloader。

### 2.1 创建目录结构

```
test_ota_stm32f411/
├── app/                          # App 工程（已有，后续改造）
├── bootloader/                   # Bootloader 工程（新建）
│   ├── boot_config.h             # 分区地址宏定义
│   ├── flash_if.h / flash_if.c   # Flash 操作封装
│   ├── ota_params.h / ota_params.c  # OTA 参数管理
│   ├── ymodem.h / ymodem.c       # YMODEM 协议
│   └── boot_main.c               # Bootloader 入口
├── mdk/
│   ├── bootloader.sct            # Bootloader 分散加载文件（新建）
│   └── app_a.sct                 # App A 分散加载文件（新建）
└── tools/
    └── ymodem_send.py            # PC 端发送脚本
```

### 2.2 创建分散加载文件

**`mdk/bootloader.sct`** — Bootloader 链接脚本：

```c
LR_IROM1 0x08000000 0x0000C000  {    ; 48KB, 起始地址 0x0800 0000
  ER_IROM1 0x08000000 0x0000C000  {
    *.o (RESET, +First)
    startup_*.o (+RO)
    .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00020000  {  ; 128KB SRAM
    .ANY (+RW +ZI)
  }
}
```

**`mdk/app_a.sct`** — App A 链接脚本：

```c
LR_IROM1 0x08010000 0x00030000  {    ; 192KB, 起始地址 0x0801 0000
  ER_IROM1 0x08010000 0x00030000  {
    *.o (RESET, +First)
    startup_*.o (+RO)
    .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x00020000  {  ; 128KB SRAM
    .ANY (+RW +ZI)
  }
}
```

> **注意**：Keil MDK 中，Linker 页的 `Scatter File` 选 `.sct` 文件；`IROM1` 起始地址和大小要与 `.sct` 一致。

### 2.3 创建 Keil Target

在 `stm32f411.uvprojx` 中添加两个 Target（或者创建两个工程）：

| Target | Scatter File | 说明 |
|--------|-------------|------|
| Bootloader | `mdk\bootloader.sct` | 编译后烧录到 0x0800 0000 |
| App | `mdk\app_a.sct` | 编译后通过 YMODEM 发送升级 |

#### 如何添加 Target（Keil MDK）

1. **Project** → **Manage** → **Project Items**
2. 在 "Project Targets" 中：默认有一个 Target 1，改名为 `Bootloader`，新建 `App`
3. **Flash** → **Configure Flash Tools** → 分别选不同 Target，设置不同的 Scatter File
4. **Output** 页面：Select Target，选中 "Create HEX File"（用于 Python 脚本发送）

> **关键**：两个 Target 使用不同的 C/C++ 宏区分：
>   - Bootloader Target：Define `BOOTLOADER`
>   - App Target：不定义此宏
> 这样 `boot_config.h` 中可以通过 `#ifdef BOOTLOADER` 区分代码路径。

### 2.4 创建 `boot_config.h`

```c
#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

#include <stdint.h>

// ======================== Flash 布局 ========================
#define FLASH_BASE_ADDR         0x08000000U
#define FLASH_TOTAL_SIZE        0x00080000U  // 512KB

// Bootloader: 48KB (Sector 0-2, 各16KB)
#define BOOTLOADER_ADDR         0x08000000U
#define BOOTLOADER_SIZE         0x0000C000U  // 48KB
#define BOOTLOADER_SECTOR_NUM   3            // Sector 0,1,2

// OTA Parameter: 16KB (Sector 3)
#define OTA_PARAM_ADDR          0x0800C000U
#define OTA_PARAM_SIZE          0x00004000U  // 16KB
#define OTA_PARAM_SECTOR        3            // Sector 3

// App A: 192KB (Sector 4:64KB + Sector 5:128KB)
#define APP_A_ADDR              0x08010000U
#define APP_A_SIZE              0x00030000U  // 192KB
#define APP_A_SECTOR_START      4
#define APP_A_SECTOR_END        5

// App B: 256KB (Sector 6:128KB + Sector 7:128KB)
#define APP_B_ADDR              0x08040000U
#define APP_B_SIZE              0x00040000U  // 256KB
#define APP_B_SECTOR_START      6
#define APP_B_SECTOR_END        7

// YMODEM 参数
#define YMODEM_PACKET_SIZE      1024        // YMODEM-1K
#define YMODEM_TIMEOUT_MS       3000        // 接收超时 (ms)
#define YMODEM_MAX_RETRIES      10          // 最大重试次数

// Bootloader 启动等待
#define BOOT_WAIT_TIMEOUT_MS    3000        // 等待 OTA 命令的超时 (ms)

// 回滚参数
#define MAX_BOOT_ATTEMPTS        3          // 最大启动尝试次数

// ======================== 宏工具 ========================
#define APP_A_ACTIVE            0
#define APP_B_ACTIVE            1

#define OTA_STATE_IDLE          0
#define OTA_STATE_RECEIVING     1
#define OTA_STATE_VERIFY        2
#define OTA_STATE_COMPLETE      3
#define OTA_STATE_FAILED        4

#define OTA_MAGIC               0x4F544152  // "RATO"

#endif
```

### 2.5 创建 Bootloader 入口 `boot_main.c`（初始版）

先写一个最小版本，验证工程可编译：

```c
#include "stm32f4xx.h"
#include "boot_config.h"
#include <stdio.h>
#include <string.h>

// ===== 串口初始化（复用 driver/usart.c 中的函数或自行初始化） =====
static void uart_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitTypeDef g;
    GPIO_StructInit(&g);
    g.GPIO_Mode  = GPIO_Mode_AF;
    g.GPIO_OType = GPIO_OType_PP;
    g.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
    g.GPIO_PuPd  = GPIO_PuPd_UP;
    g.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOA, &g);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    USART_InitTypeDef u;
    USART_StructInit(&u);
    u.USART_BaudRate = 115200;
    u.USART_WordLength = USART_WordLength_8b;
    u.USART_StopBits = USART_StopBits_1;
    u.USART_Parity = USART_Parity_No;
    u.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &u);
    USART_Cmd(USART1, ENABLE);
}

// printf 重定向（如果 driver/usart.c 中已定义 fputc，需要确保编译进去）
int fputc(int ch, FILE *f) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint8_t)ch);
    return ch;
}

// ===== 简单延时（SysTick-based，后续可改用定时器） =====
static void delay_ms(uint32_t ms)
{
    // 用 SysTick 作粗略延时（100MHz 主频，1ms = 100000 tick）
    SysTick->LOAD = 100000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    for (uint32_t i = 0; i < ms; i++) {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
    }
    SysTick->CTRL = 0;
}

int main(void)
{
    uart_init();
    printf("\r\n================================\r\n");
    printf("Bootloader v1.0 (STM32F411)\r\n");
    printf("Flash: %dKB (0x%08X - 0x%08X)\r\n",
           FLASH_TOTAL_SIZE / 1024, FLASH_BASE_ADDR,
           FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - 1);
    printf("================================\r\n\r\n");

    printf("[BOOT] Phase 1 check: Bootloader running OK\r\n");

    // 接下来是各阶段的逻辑占位
    while (1)
    {
        // TODO: 阶段二 — 跳转 App
        // TODO: 阶段六 — YMODEM 接收
    }
}
```

### 阶段一检查点

编译 Bootloader Target，烧录到 Flash `0x0800 0000`，串口应输出：

```
================================
Bootloader v1.0 (STM32F411)
Flash: 512KB (0x08000000 - 0x0807FFFF)
================================
[BOOT] Phase 1 check: Bootloader running OK
```

### 阶段一常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| 编译失败：`Undefined symbol` | 标准外设库文件未加入工程 | Keil 中添加 `stm32f4xx_flash.c`, `stm32f4xx_rcc.c` 等到工程 |
| 烧录后无输出 | 时钟/串口配置错误 | 检查 `system_stm32f4xx.c` 中 `SystemCoreClock = 100000000` |
| 烧录后无输出 | 串口线接错 | TX→RX, RX→TX, GND→GND |
| printf 无法编译 | 缺少 `stdio.h` | 添加 `#include <stdio.h>` |

---

## 三、阶段二：Bootloader 骨架

> **目标**：Bootloader 能判断当前应该进入升级模式还是跳转 App。

### 3.1 实现跳转 App 函数

在 `boot_main.c` 中添加：

```c
// ===== 跳转到 App =====
typedef void (*app_entry_t)(void);

static void jump_to_app(uint32_t app_addr)
{
    printf("[BOOT] Jumping to App at 0x%08X...\r\n", (unsigned int)app_addr);

    // 1. 读取 App 的栈指针和复位向量
    uint32_t sp = *((volatile uint32_t *)app_addr);
    uint32_t pc = *((volatile uint32_t *)(app_addr + 4));

    printf("[BOOT]   SP = 0x%08X, PC = 0x%08X\r\n", (unsigned int)sp, (unsigned int)pc);

    // 2. 校验栈指针是否在 SRAM 范围内
    if ((sp & 0x2FFE0000) != 0x20000000) {
        printf("[BOOT] ERROR: Invalid stack pointer! Abort jump.\r\n");
        return;
    }

    // 3. 关闭所有中断（清理 Bootloader 的配置）
    __disable_irq();

    // 4. 停止 SysTick（否则 App 中会收到 SysTick 中断）
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // 5. 将所有已使能的中断清除挂起标志
    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  // 禁用中断
        NVIC->ICPR[i] = 0xFFFFFFFF;  // 清除挂起标志
    }

    // 6. 设置新的向量表地址（App 的 SystemInit 会重新设置，
    //    但为了安全，Bootloader 这里先设好）
    SCB->VTOR = app_addr & 0xFFFFFF00;  // VTOR 对齐 0x100

    // 7. 设置栈指针并跳转
    __set_MSP(sp);
    __set_PSP(sp);  // 可选：如果 App 使用 PSP

    app_entry_t entry = (app_entry_t)pc;
    entry();

    // 永远不会到这里
}
```

### 3.2 验证 Flash 分区内容

在 `boot_main.c` 中添加：

```c
// ===== 简便的 Flash 检查 =====
static void dump_flash_range(uint32_t addr, uint32_t len)
{
    printf("[BOOT] Flash @ 0x%08X (%u bytes):\r\n", (unsigned int)addr, (unsigned int)len);
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t val = *((volatile uint32_t *)(addr + i));
        if (i % 16 == 0) printf("  %08X: ", (unsigned int)(addr + i));
        printf("%08X ", (unsigned int)val);
        if (i % 16 == 12) printf("\r\n");
    }
    printf("\r\n");
}
```

### 3.3 主循环：等待 + 跳转

更新 `main()` 函数：

```c
int main(void)
{
    uart_init();
    printf("\r\n================================\r\n");
    printf("Bootloader v1.0\r\n");
    printf("================================\r\n\r\n");

    // ====== 阶段二：简单的等待+跳转逻辑 ======
    // 1. 检查 App A 是否有效
    uint32_t app_a_sp = *((volatile uint32_t *)APP_A_ADDR);
    uint32_t app_a_pc = *((volatile uint32_t *)(APP_A_ADDR + 4));

    printf("[BOOT] App A @ 0x%08X: SP=0x%08X PC=0x%08X\r\n",
           (unsigned int)APP_A_ADDR, (unsigned int)app_a_sp, (unsigned int)app_a_pc);

    // 2. 如果栈指针合法（在 SRAM 范围内），跳转 App A
    if ((app_a_sp & 0x2FFE0000) == 0x20000000 && app_a_pc != 0xFFFFFFFF) {
        printf("[BOOT] App A looks valid, jumping...\r\n");
        delay_ms(500);
        jump_to_app(APP_A_ADDR);
    } else {
        printf("[BOOT] App A invalid, staying in bootloader.\r\n");
        printf("[BOOT] Waiting for YMODEM upgrade...\r\n");
    }

    // 3. 停留在 Bootloader，等待升级命令
    while (1) {
        // TODO: 阶段六 — YMODEM 升级逻辑
        delay_ms(1000);
        printf("[BOOT] Waiting...\r\n");
    }
}
```

### 阶段二检查点

烧录 Bootloader 后，如果之前 App 区（0x0801 0000）是空白的（全 0xFF）：

```
================================
Bootloader v1.0
================================
[BOOT] App A @ 0x08010000: SP=0xFFFFFFFF PC=0xFFFFFFFF
[BOOT] App A invalid, staying in bootloader.
[BOOT] Waiting for YMODEM upgrade...
[BOOT] Waiting...
```

如果 App A 有效（先烧录了一个 App），应看到：

```
[BOOT] App A @ 0x08010000: SP=0x2000XXXX PC=0x0801XXXX
[BOOT] App A looks valid, jumping...
[BOOT] Jumping to App at 0x08010000...
[BOOT]   SP = 0x2000XXXX, PC = 0x0801XXXX
```

### 阶段二常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| 跳转后 HardFault | App 未设置 `VECT_TAB_OFFSET` | 阶段八才修改 App，先让跳转逻辑正确即可 |
| 跳转后卡死无输出 | 跳转前未关闭中断 | 确保 `__disable_irq()` 和 SysTick 关闭 |
| 跳转后串口无输出 | App 重新初始化了串口但时钟不同 | App 的 `SystemInit()` 会重新配时钟 |

---

## 四、阶段三：Flash 操作层

> **目标**：封装 Flash 扇区擦除和字写入函数，并提供测试函数验证。

### 4.1 创建 `bootloader/flash_if.h`

```c
#ifndef __FLASH_IF_H
#define __FLASH_IF_H

#include <stdint.h>
#include "boot_config.h"

// 初始化（解锁 Flash）
int  flash_if_init(void);

// 锁定 Flash
void flash_if_lock(void);

// 擦除多个扇区
int  flash_if_erase(uint32_t start_addr, uint32_t size);

// 按 32 位写入（每次写 4 字节）
int  flash_if_write_word(uint32_t addr, uint32_t data);

// 批量写入
int  flash_if_write(uint32_t addr, const uint8_t *data, uint32_t len);

// 获取 Key 值（STM32F4 Flash 解锁需要）
uint32_t flash_if_get_sector(uint32_t addr);

#endif
```

### 4.2 创建 `bootloader/flash_if.c`

```c
#include "flash_if.h"
#include "stm32f4xx_flash.h"
#include <string.h>

// ===== 初始化 =====
int flash_if_init(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |
                    FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    return 0;  // 成功
}

void flash_if_lock(void)
{
    FLASH_Lock();
}

// ===== 获取扇区编号 =====
// 此函数根据地址返回 FLASH_Sector_n 常量
// STM32F4 标准外设库中：
//   FLASH_Sector_0 = 0x00 (0x0800 0000 - 0x0800 3FFF)
//   FLASH_Sector_1 = 0x08 (0x0800 4000 - 0x0800 7FFF)
//   ...
//   FLASH_Sector_4 = 0x20 (0x0801 0000 - 0x0801 FFFF)
//   FLASH_Sector_5 = 0x28 (0x0802 0000 - 0x0803 FFFF)
//   等等
uint32_t flash_if_get_sector(uint32_t addr)
{
    if (addr < 0x08004000)  return FLASH_Sector_0;  // 0x0000 - 0x3FFF
    if (addr < 0x08008000)  return FLASH_Sector_1;  // 0x4000 - 0x7FFF
    if (addr < 0x0800C000)  return FLASH_Sector_2;  // 0x8000 - 0xBFFF
    if (addr < 0x08010000)  return FLASH_Sector_3;  // 0xC000 - 0xFFFF
    if (addr < 0x08020000)  return FLASH_Sector_4;  // 0x10000 - 0x1FFFF
    if (addr < 0x08040000)  return FLASH_Sector_5;  // 0x20000 - 0x3FFFF
    if (addr < 0x08060000)  return FLASH_Sector_6;  // 0x40000 - 0x5FFFF
    /* addr < 0x08080000 */  return FLASH_Sector_7;  // 0x60000 - 0x7FFFF
}

// ===== 擦除扇区 =====
// start_addr: 擦除起始地址
// size:       擦除字节数
// 返回 0 表示成功，-1 表示失败
int flash_if_erase(uint32_t start_addr, uint32_t size)
{
    uint32_t end_addr = start_addr + size;
    uint32_t cur_addr = start_addr;

    // 按扇区逐个擦除
    while (cur_addr < end_addr) {
        uint32_t sector = flash_if_get_sector(cur_addr);

        if (FLASH_EraseSector(sector, VoltageRange_3) != FLASH_COMPLETE) {
            return -1;  // 擦除失败
        }

        // 根据扇区大小推进地址
        if (sector == FLASH_Sector_0 || sector == FLASH_Sector_1 ||
            sector == FLASH_Sector_2 || sector == FLASH_Sector_3) {
            cur_addr += 0x4000;   // 16KB
        } else if (sector == FLASH_Sector_4) {
            cur_addr += 0x10000;  // 64KB
        } else {
            cur_addr += 0x20000;  // 128KB
        }
    }

    return 0;
}

// ===== 写入一个字（4字节） =====
int flash_if_write_word(uint32_t addr, uint32_t data)
{
    if (FLASH_ProgramWord(addr, data) != FLASH_COMPLETE) {
        return -1;
    }
    // 校验写入
    if (*(volatile uint32_t *)addr != data) {
        return -1;
    }
    return 0;
}

// ===== 批量写入（按 4 字节对齐） =====
int flash_if_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    // 按字节写入直到 4 字节对齐
    while (len > 0 && (addr & 0x03)) {
        // 使用 FLASH_ProgramByte 或读-改-写整个 Word
        // 简化处理：用 byte 编程
        if (FLASH_ProgramByte(addr, *data) != FLASH_COMPLETE) {
            return -1;
        }
        addr++; data++; len--;
    }

    // 按字写入
    for (i = 0; i < len / 4; i++) {
        uint32_t word = ((uint32_t)data[0] <<  0) |
                        ((uint32_t)data[1] <<  8) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24);
        if (flash_if_write_word(addr + i * 4, word) != 0) {
            return -1;
        }
        data += 4;
    }

    // 剩余字节
    uint32_t remaining = len % 4;
    addr += (len / 4) * 4;
    for (i = 0; i < remaining; i++) {
        if (FLASH_ProgramByte(addr + i, data[i]) != FLASH_COMPLETE) {
            return -1;
        }
    }

    return 0;
}
```

### 4.3 测试函数

在 `boot_main.c` 中添加测试代码（阶段三验证用）：

```c
// 阶段三测试：擦除并写入 OTA Parameter 区
static void test_flash_ops(void)
{
    printf("[TEST] Flash operations test...\r\n");

    // 1. 初始化（解锁）
    flash_if_init();
    printf("[TEST] Flash unlocked.\r\n");

    // 2. 擦除 OTA Parameter 区
    printf("[TEST] Erasing OTA Parameter sector...\r\n");
    if (flash_if_erase(OTA_PARAM_ADDR, OTA_PARAM_SIZE) == 0) {
        printf("[TEST] Erase OK.\r\n");
    } else {
        printf("[TEST] Erase FAILED!\r\n");
        return;
    }

    // 3. 验证擦除（全 0xFF）
    uint32_t *p = (uint32_t *)OTA_PARAM_ADDR;
    int erased_ok = 1;
    for (int i = 0; i < 4; i++) {
        if (p[i] != 0xFFFFFFFF) {
            printf("[TEST] Not erased at offset %d: 0x%08X\r\n", i, (unsigned int)p[i]);
            erased_ok = 0;
        }
    }
    if (erased_ok) printf("[TEST] Erase verification OK (all 0xFF).\r\n");

    // 4. 写入测试数据
    uint32_t test_data[] = { 0x4F544152, 0x00010000, 0x00000000, 0x00000000 };  // "RATO", v1.0.0
    printf("[TEST] Writing test data...\r\n");
    if (flash_if_write(OTA_PARAM_ADDR, (uint8_t *)test_data, sizeof(test_data)) == 0) {
        printf("[TEST] Write OK.\r\n");
    } else {
        printf("[TEST] Write FAILED!\r\n");
    }

    // 5. 回读验证
    printf("[TEST] Readback: ");
    for (int i = 0; i < 4; i++) {
        printf("0x%08X ", (unsigned int)p[i]);
    }
    printf("\r\n");

    // 6. 锁定
    flash_if_lock();
    printf("[TEST] Flash test DONE.\r\n");
}
```

### 阶段三检查点

调用 `test_flash_ops()` 后串口输出：

```
[TEST] Flash operations test...
[TEST] Flash unlocked.
[TEST] Erasing OTA Parameter sector...
[TEST] Erase OK.
[TEST] Erase verification OK (all 0xFF).
[TEST] Writing test data...
[TEST] Write OK.
[TEST] Readback: 0x4F544152 0x00010000 0x00000000 0x00000000
[TEST] Flash test DONE.
```

### 阶段三常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| Flash 操作卡死 | 执行前未调 `FLASH_Unlock()` | 在擦/写前必须 unlock |
| 擦除返回错误 | 擦除过程中执行了 Flash 中的代码 | STM32F4 读 Flash 和写 Flash 冲突。**擦写期间代码不能访问被操作的 Flash bank** |
| 擦除返回错误 | `VoltageRange` 参数不匹配 | F411 的 `SystemInit()` 设置 Scale1 → 用 `VoltageRange_3`（对应 2.7–3.6V） |
| 写入后读回不对 | 未擦除直接写 | Flash 写只能 1→0，必须先擦除（擦除是 0→1） |
| 擦除后 Bootloader 卡死 | 犯了"在 Bootloader 区擦除 Bootloader 区"的错误 | 确保擦除地址不在 0x08000000–0x0800BFFF |

---

## 五、阶段四：CRC32 校验

> **目标**：实现固件的 CRC32 校验，用于传输后验证固件完整性。

### 5.1 创建 `bootloader/ota_params.h`

```c
#ifndef __OTA_PARAMS_H
#define __OTA_PARAMS_H

#include <stdint.h>
#include "boot_config.h"

// ===== OTA 参数结构体（存储在 0x0800 C000） =====
typedef struct __attribute__((packed)) {
    uint32_t magic;                 // 魔数 0x4F544152 ("RATO")
    uint32_t param_version;         // 参数版本号

    uint8_t  active_partition;      // 当前活跃分区: 0=App A, 1=App B
    uint8_t  ota_state;             // OTA 状态: 0=IDLE, 1=RECEIVING,
                                    //            2=VERIFY, 3=COMPLETE, 4=FAILED
    uint8_t  boot_count;            // 启动尝试计数
    uint8_t  max_boot_count;        // 最大启动尝试次数

    // App A 固件信息
    uint32_t app_a_version;         // 版本号 (0x00010000 = v1.0.0)
    uint32_t app_a_size;            // 固件大小 (bytes)
    uint32_t app_a_crc32;           // CRC32 校验值

    // App B 固件信息
    uint32_t app_b_version;
    uint32_t app_b_size;
    uint32_t app_b_crc32;

    // 保留（可用于扩展）
    uint32_t reserved[4];
} ota_param_t;

// CRC32 函数
uint32_t crc32_calc(const uint8_t *data, uint32_t len);
uint32_t crc32_flash(uint32_t addr, uint32_t len);

// 参数管理函数（阶段五实现）
int      ota_params_init(void);
int      ota_params_load(ota_param_t *param);
int      ota_params_save(const ota_param_t *param);

#endif
```

### 5.2 创建 `bootloader/ota_params.c`

```c
#include "ota_params.h"
#include "flash_if.h"
#include "boot_config.h"
#include <string.h>

// ===== CRC32 查表（标准 CRC-32 / MPEG-2） =====
// 多项式: 0xEDB88320 (反射)
static const uint32_t crc32_table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,
    0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
    0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
    0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,
    0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
    0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
    0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
    0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,
    0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,
    0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
    0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
    0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,
    0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88F,0xE10E9818,
    0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
    0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
    0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,
    0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,
    0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
    0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
    0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,
    0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,
    0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
    0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
    0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
    0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,
    0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
    0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
    0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,
    0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,
    0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
    0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
    0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB30A04,
    0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
    0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
    0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,
    0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,
    0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
    0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
    0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,
    0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,
    0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
    0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
    0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

// ===== 计算 CRC32 =====
uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ===== 计算 Flash 中数据的 CRC32 =====
uint32_t crc32_flash(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t *p = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}
```

### 阶段四测试代码

```c
// 阶段四测试：CRC32 校验
static void test_crc32(void)
{
    const uint8_t test_data[] = "123456789";
    uint32_t crc = crc32_calc(test_data, 9);

    // 标准 CRC-32("123456789") = 0xCBF43926
    printf("[TEST] CRC32('123456789') = 0x%08X (expected: 0xCBF43926)\r\n",
           (unsigned int)crc);
    if (crc == 0xCBF43926) {
        printf("[TEST] CRC32 test PASS.\r\n");
    } else {
        printf("[TEST] CRC32 test FAIL!\r\n");
    }
}
```

### 阶段四检查点

```
[TEST] CRC32('123456789') = 0xCBF43926 (expected: 0xCBF43926)
[TEST] CRC32 test PASS.
```

---

## 六、阶段五：OTA Parameter 管理

> **目标**：实现 OTA 参数区（0x0800 C000）的读写管理和初始化。

### 6.1 实现参数管理函数

在前面的 `bootloader/ota_params.c` 中，添加参数管理函数：

```c
// ===== CRC32 实现和初始化 =====

// ===== 参数管理函数 =====
// 初始化参数区（首次上电或 magic 不匹配时调用）
int ota_params_init(void)
{
    ota_param_t param;
    memset(&param, 0, sizeof(param));

    param.magic              = OTA_MAGIC;
    param.param_version      = 0x00010000;        // v1.0.0
    param.active_partition   = APP_A_ACTIVE;       // 默认运行 App A
    param.ota_state          = OTA_STATE_IDLE;
    param.boot_count         = 0;
    param.max_boot_count     = MAX_BOOT_ATTEMPTS;  // 默认 3 次

    // App 区初始信息写 0（表示无效）
    param.app_a_version      = 0;
    param.app_a_size         = 0;
    param.app_a_crc32        = 0;
    param.app_b_version      = 0;
    param.app_b_size         = 0;
    param.app_b_crc32        = 0;

    // 擦除参数区
    flash_if_init();
    if (flash_if_erase(OTA_PARAM_ADDR, OTA_PARAM_SIZE) != 0) {
        flash_if_lock();
        return -1;
    }

    // 写入参数
    if (flash_if_write(OTA_PARAM_ADDR, (uint8_t *)&param, sizeof(param)) != 0) {
        flash_if_lock();
        return -1;
    }

    flash_if_lock();
    return 0;
}

// 从 Flash 加载参数
int ota_params_load(ota_param_t *param)
{
    memcpy(param, (void *)OTA_PARAM_ADDR, sizeof(ota_param_t));
    return 0;
}

// 保存参数到 Flash（需要先擦除、写入）
int ota_params_save(const ota_param_t *param)
{
    flash_if_init();

    if (flash_if_erase(OTA_PARAM_ADDR, OTA_PARAM_SIZE) != 0) {
        flash_if_lock();
        return -1;
    }

    if (flash_if_write(OTA_PARAM_ADDR, (uint8_t *)param, sizeof(ota_param_t)) != 0) {
        flash_if_lock();
        return -1;
    }

    flash_if_lock();
    return 0;
}
```

### 6.2 主流程中读取参数

更新 `boot_main.c` 的 `main()`：

```c
ota_param_t g_ota_param;  // 全局 OTA 参数

int main(void)
{
    uart_init();
    printf("\r\n================================\r\n");
    printf("Bootloader v1.0\r\n");
    printf("================================\r\n\r\n");

    // ===== 阶段五：加载/初始化 OTA 参数 =====
    ota_params_load(&g_ota_param);

    if (g_ota_param.magic != OTA_MAGIC) {
        printf("[BOOT] OTA param magic mismatch (0x%08X), initializing...\r\n",
               (unsigned int)g_ota_param.magic);
        if (ota_params_init() != 0) {
            printf("[BOOT] OTA param init FAILED!\r\n");
            while (1);
        }
        ota_params_load(&g_ota_param);
        printf("[BOOT] OTA param initialized.\r\n");
    } else {
        printf("[BOOT] OTA param loaded OK.\r\n");
    }

    printf("[BOOT] Active partition: %s\r\n",
           g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B");
    printf("[BOOT] OTA state: %d\r\n", g_ota_param.ota_state);
    printf("[BOOT] App A ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_a_version,
           (unsigned int)g_ota_param.app_a_size);
    printf("[BOOT] App B ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_b_version,
           (unsigned int)g_ota_param.app_b_size);

    // ===== 跳转逻辑（阶段七完善） =====
    uint32_t target_addr = (g_ota_param.active_partition == APP_A_ACTIVE)
                           ? APP_A_ADDR : APP_B_ADDR;

    uint32_t sp = *((volatile uint32_t *)target_addr);
    if ((sp & 0x2FFE0000) == 0x20000000) {
        printf("[BOOT] Valid app found, jumping to 0x%08X...\r\n",
               (unsigned int)target_addr);
        jump_to_app(target_addr);
    } else {
        printf("[BOOT] No valid app, entering bootloader mode.\r\n");
    }

    printf("[BOOT] Waiting for YMODEM upgrade...\r\n");
    while (1) {
        delay_ms(1000);
    }
}
```

### 阶段五检查点

首次运行（参数区空白）：

```
[BOOT] OTA param magic mismatch (0xFFFFFFFF), initializing...
[BOOT] OTA param initialized.
[BOOT] Active partition: App A
[BOOT] OTA state: 0
[BOOT] App A ver: 0x00000000, size: 0
```

再次运行（参数区已有数据）：

```
[BOOT] OTA param loaded OK.
[BOOT] Active partition: App A
```

---

## 七、阶段六：YMODEM 协议实现

> **目标**：Bootloader 通过 USART1 接收 YMODEM-1K 协议发送的固件。

### 7.1 YMODEM 协议回顾

YMODEM 协议帧格式：

```
+------+------+--------+---------+--------+
| SOH  | 序号  | 序号反码 | 数据(1KB) | CRC16  |
| 0x01 | 0xNN | 0xFF-NN | 1024字节 | 2字节  |
+------+------+---------+---------+--------+
```

流程：

```
PC 端                               MCU 端
  │                                   │
  │ ←───── 'C' (轮询，等待文件名包) ──│
  │                                   │
  │ ── 文件名包(序号0):文件名+大小 ──→│
  │                                   │
  │ ←──────────── ACK ──────────────│
  │ ──────── 第二个包(序号0) ──────→│  ← 同序号，确认传输参数
  │ ←──────────── ACK ──────────────│
  │                                   │
  │ ←───── 'C' (请求第1个数据包) ───│
  │ ──────── 数据包(序号1) ────────→│
  │ ←──────────── ACK ──────────────│
  │       ... ... ... ...            │
  │ ──────── 最后数据包 ───────────→│
  │ ←──────────── ACK ──────────────│
  │                                   │
  │ ──────────── EOT ──────────────→│
  │ ←──────────── NAK ──────────────│
  │ ──────────── EOT ──────────────→│  ← 第二次 EOT
  │ ←──────────── ACK ──────────────│
  │                                   │
  │ ───── 空文件名(全0) ───────────→│  ← 传输结束
  │ ←──────────── ACK ──────────────│
```

### 7.2 YMODEM 常量定义

```c
// YMODEM 控制字符
#define SOH         0x01   // 128 字节包
#define STX         0x02   // 1024 字节包 (YMODEM-1K)
#define EOT         0x04   // 传输结束
#define ACK         0x06   // 确认
#define NAK         0x15   // 未确认（重传请求）
#define CAN         0x18   // 取消传输
#define C_CHAR      0x43   // 'C'，请求 YMODEM CRC16

// YMODEM 包结构
#define PKT_HEADER  3      // SOH/STX + 序号 + 反码
#define PKT_DATA_1K 1024   // 1KB 数据
#define PKT_DATA_128 128   // 128B 数据
#define PKT_CRC     2      // 2 字节 CRC16

#define PKT_SIZE_1K (PKT_HEADER + PKT_DATA_1K + PKT_CRC)  // 1029
```

### 7.3 创建 `bootloader/ymodem.h`

```c
#ifndef __YMODEM_H
#define __YMODEM_H

#include <stdint.h>

#define YMODEM_OK             0
#define YMODEM_ERR_TIMEOUT   -1
#define YMODEM_ERR_CRC       -2
#define YMODEM_ERR_SEQ       -3
#define YMODEM_ERR_FLASH     -4
#define YMODEM_ERR_CANCEL    -5
#define YMODEM_ERR_FILESIZE  -6

typedef struct {
    uint8_t  file_name[64];
    uint32_t file_size;
    uint32_t total_received;
    uint32_t packet_count;
    int      error_code;
} ymodem_status_t;

int      ymodem_receive(uint32_t target_addr, uint32_t max_size,
                        ymodem_status_t *status);

#endif
```

### 7.4 创建 `bootloader/ymodem.c`

```c
#include "ymodem.h"
#include "flash_if.h"
#include "ota_params.h"
#include "boot_config.h"
#include "stm32f4xx_usart.h"
#include <string.h>
#include <stdio.h>

// ===== 串口字节收发 =====
static int uart_putc(uint8_t c) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, c);
    return 0;
}

static int uart_getc_timeout(uint8_t *c, uint32_t timeout_ms) {
    // 简单轮询方式，200 次/ms
    uint32_t cnt = timeout_ms * 200;
    while (cnt--) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) {
            *c = USART_ReceiveData(USART1);
            return 1;  // 收到
        }
    }
    return 0;  // 超时
}

static void uart_flush_rx(void) {
    uint8_t dummy;
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) {
        USART_ReceiveData(USART1);
    }
}

// ===== CRC16（YMODEM 用） =====
// 多项式 0x1021，初始值 0x0000
static uint16_t crc16_calc(const uint8_t *data, uint32_t len) {
    uint16_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

// ===== 接收一包数据 =====
// buf: 接收缓冲区 (≥ 1024 字节)
// 实际收到的包大小可能为 128 或 1024
// 返回: 实际数据大小，<0 表示错误
static int ymodem_recv_packet(uint8_t *buf, uint8_t *pkt_seq,
                              uint32_t timeout_ms)
{
    (void)timeout_ms;

    // 等待 SOH/STX/EOT/CAN
    uint8_t hdr;
    if (!uart_getc_timeout(&hdr, YMODEM_TIMEOUT_MS)) {
        return YMODEM_ERR_TIMEOUT;
    }

    if (hdr == EOT) {
        return 0;  // 传输结束，不是数据包
    }
    if (hdr == CAN) {
        return YMODEM_ERR_CANCEL;
    }
    if (hdr != SOH && hdr != STX) {
        // 不明包类型，丢弃
        return -1;
    }

    // 包大小
    uint16_t data_len = (hdr == STX) ? 1024 : 128;

    // 接收序号
    uint8_t seq, seq_inv;
    if (!uart_getc_timeout(&seq, YMODEM_TIMEOUT_MS))     return YMODEM_ERR_TIMEOUT;
    if (!uart_getc_timeout(&seq_inv, YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;

    // 校验序号
    if ((uint8_t)(seq + seq_inv) != 0xFF) {
        return YMODEM_ERR_SEQ;
    }

    *pkt_seq = seq;

    // 接收数据 + CRC16
    uint8_t crc_buf[2];
    for (uint16_t i = 0; i < data_len; i++) {
        if (!uart_getc_timeout(&buf[i], YMODEM_TIMEOUT_MS))
            return YMODEM_ERR_TIMEOUT;
    }
    if (!uart_getc_timeout(&crc_buf[0], YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;
    if (!uart_getc_timeout(&crc_buf[1], YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;

    // 校验 CRC
    uint16_t crc_rx = ((uint16_t)crc_buf[0] << 8) | crc_buf[1];
    uint16_t crc_cal = crc16_calc(buf, data_len);
    if (crc_rx != crc_cal) {
        return YMODEM_ERR_CRC;
    }

    return data_len;
}

// ===== 解析文件名包（序号 0） =====
// YMODEM 文件名包的 128 字节中：
//   0-63: 文件名（以 0x00 结尾）
//   后续: 文件大小（ASCII 字符串，以 0x20 或 0x00 结尾）
//   其余补 0
static int parse_filename_packet(uint8_t *buf, ymodem_status_t *status)
{
    // 提取文件名
    int name_len = 0;
    while (name_len < 64 && buf[name_len] != 0x00) name_len++;
    if (name_len >= 64 || name_len == 0) return -1;

    memcpy(status->file_name, buf, name_len);
    status->file_name[name_len] = '\0';

    // 跳过文件名和结束符
    int pos = name_len + 1;

    // 提取文件大小（ASCII 数字 + 空格）
    char size_str[16] = {0};
    int s = 0;
    while (pos < 128 && buf[pos] != 0x00 && buf[pos] != 0x20 && s < 15) {
        size_str[s++] = buf[pos++];
    }
    size_str[s] = '\0';

    status->file_size = 0;
    for (int i = 0; i < s; i++) {
        if (size_str[i] >= '0' && size_str[i] <= '9')
            status->file_size = status->file_size * 10 + (size_str[i] - '0');
    }

    return 0;
}

// ===== YMODEM 接收主函数 =====
int ymodem_receive(uint32_t target_addr, uint32_t max_size,
                   ymodem_status_t *status)
{
    uint8_t *pkt_buf = (uint8_t *)PKT_DATA_1K;  // 实际使用大缓冲区
    // 实际需要 malloc 或静态缓冲区
    static uint8_t rx_buf[1024];  // 接收数据缓冲
    // 写入缓冲（4KB = 一个 1024 包的倍数）
    static uint8_t wr_buf[4096];
    uint32_t wr_offset = 0;
    uint32_t flash_addr = target_addr;

    int retries = 0;
    uint8_t expected_seq = 0;
    int state = 0;  // 0=等待文件名, 1=接收数据, 2=EOT, 3=结束
                    // STM32F4 Volta
                    // STM32F4 Volta
    printf("[YMODEM] Starting receive, target=0x%08X max=%u\r\n",
           (unsigned int)target_addr, (unsigned int)max_size);

    // 清空串口缓冲区
    uart_flush_rx();

    // ===== Step 1: 发送 'C'，请求文件名包 =====
    printf("[YMODEM] Sending 'C'...\r\n");
    for (int i = 0; i < 60; i++) {  // 最多发送 60 次，每次间隔 1 秒
        uart_putc(C_CHAR);
        // 延时约 1 秒（用串口发送做粗略延时）
        // 也可用 delay_ms(1000)，约每 50ms 发一个 C
        for (volatile int d = 0; d < 100000; d++);

        // 尝试接收
        uint8_t seq;
        int len = ymodem_recv_packet(rx_buf, &seq, 0);
        if (len >= 0) {
            if (seq == 0 && len > 0) {
                // 收到文件名包
                if (parse_filename_packet(rx_buf, status) == 0) {
                    printf("[YMODEM] File: %s, Size: %u bytes\r\n",
                           status->file_name,
                           (unsigned int)status->file_size);
                    if (status->file_size > max_size) {
                        printf("[YMODEM] ERROR: File too large (%u > %u)!\r\n",
                               (unsigned int)status->file_size,
                               (unsigned int)max_size);
                        return YMODEM_ERR_FILESIZE;
                    }
                    uart_putc(ACK);
                    expected_seq = 0;  // 下一个包序号 = 1
                    state = 1;
                    break;
                }
            }
        }
        if (i % 10 == 0 && i > 0) {
            printf("[YMODEM] Still waiting for filename packet...\r\n");
        }
    }

    if (state != 1) {
        printf("[YMODEM] Timeout waiting for filename packet.\r\n");
        return YMODEM_ERR_TIMEOUT;
    }

    // ===== Step 2: 接收第二个包（序号 0，确认） =====
    // 收到这个包后发送 ACK，进入数据接收
    uint8_t seq;
    int len = ymodem_recv_packet(rx_buf, &seq, YMODEM_TIMEOUT_MS);
    if (len < 0) {
        printf("[YMODEM] Second seq=0 packet error: %d\r\n", len);
        uart_putc(NAK);
        return len;
    }
    uart_putc(C_CHAR);  // 告知 PC 端开始发送数据
    expected_seq = 1;
    state = 2;

    printf("[YMODEM] Data transfer starting...\r\n");

    // ===== Step 3: 接收数据包 =====
    flash_if_init();

    // 擦除目标分区
    printf("[YMODEM] Erasing target area 0x%08X (%u sectors)...\r\n",
           (unsigned int)target_addr, (unsigned int)max_size);
    flash_if_erase(target_addr, max_size);

    status->total_received = 0;
    status->packet_count = 0;
    status->error_code = 0;

    while (state == 2) {
        len = ymodem_recv_packet(rx_buf, &seq, YMODEM_TIMEOUT_MS);

        if (len == 0) {
            // EOT
            printf("[YMODEM] EOT received, total=%u bytes\r\n",
                   (unsigned int)status->total_received);
            uart_putc(NAK);

            // 等待第二个 EOT
            len = ymodem_recv_packet(rx_buf, &seq, YMODEM_TIMEOUT_MS);
            if (len == 0) {
                uart_putc(ACK);
                state = 3;  // 完成
                printf("[YMODEM] Transfer complete.\r\n");
                break;
            }
            continue;
        }

        if (len < 0) {
            // 错误处理
            if (retries++ < YMODEM_MAX_RETRIES) {
                uart_putc(NAK);
                printf("[YMODEM] Packet error %d, retrying (%d)...\r\n",
                       len, retries);
                continue;
            } else {
                printf("[YMODEM] Max retries reached, aborting.\r\n");
                flash_if_lock();
                return len;
            }
        }

        // 校验序号
        if (seq != expected_seq) {
            printf("[YMODEM] Sequence error: expected %d got %d\r\n",
                   expected_seq, seq);
            if (seq == (expected_seq - 1)) {
                // 重复包，重发 ACK
                uart_putc(ACK);
                continue;
            }
            flash_if_lock();
            return YMODEM_ERR_SEQ;
        }

        // 写入 Flash（通过缓冲区）
        // 将接收数据写入 wr_buf，攒够 4096 字节或最后一包时写入 Flash
        // 简化：直接写入 Flash（每包 1024B）
        if (flash_if_write(flash_addr + status->total_received,
                           rx_buf, len) != 0) {
            printf("[YMODEM] Flash write error at offset %u\r\n",
                   (unsigned int)status->total_received);
            flash_if_lock();
            return YMODEM_ERR_FLASH;
        }

        status->total_received += len;
        status->packet_count++;
        expected_seq = (expected_seq + 1) & 0xFF;
        retries = 0;

        uart_putc(ACK);

        // 进度打印
        if (status->packet_count % 16 == 0) {
            printf("[YMODEM] Progress: %u/%u bytes (%u%%)\r\n",
                   (unsigned int)status->total_received,
                   (unsigned int)status->file_size,
                   (unsigned int)(status->total_received * 100 /
                                  status->file_size));
        }
    }

    flash_if_lock();
    printf("[YMODEM] Reception done: %u bytes in %u packets.\r\n",
           (unsigned int)status->total_received,
           (unsigned int)status->packet_count);
    return YMODEM_OK;
}
```

### 7.5 集成到主流程

在 `boot_main.c` 中添加：

```c
void ota_ymodem_start(void)
{
    printf("[BOOT] Starting YMODEM OTA...\r\n");

    // 确定写入目标（非活跃分区）
    uint32_t target_addr, target_size;
    if (g_ota_param.active_partition == APP_A_ACTIVE) {
        target_addr = APP_B_ADDR;
        target_size = APP_B_SIZE;
    } else {
        target_addr = APP_A_ADDR;
        target_size = APP_A_SIZE;
    }

    ymodem_status_t status;
    int ret = ymodem_receive(target_addr, target_size, &status);

    if (ret == YMODEM_OK) {
        printf("[BOOT] YMODEM transfer OK, verifying...\r\n");

        // CRC32 校验
        uint32_t crc = crc32_flash(target_addr, status.total_received);
        printf("[BOOT] CRC32: 0x%08X\r\n", (unsigned int)crc);

        // 更新 OTA 参数
        g_ota_param.ota_state = OTA_STATE_COMPLETE;
        if (target_addr == APP_B_ADDR) {
            g_ota_param.app_b_version = 0x00010001;  // 示例版本号
            g_ota_param.app_b_size = status.total_received;
            g_ota_param.app_b_crc32 = crc;
        } else {
            g_ota_param.app_a_version = 0x00010001;
            g_ota_param.app_a_size = status.total_received;
            g_ota_param.app_a_crc32 = crc;
        }
        ota_params_save(&g_ota_param);
        printf("[BOOT] OTA params updated. Rebooting...\r\n");
        NVIC_SystemReset();
    } else {
        printf("[BOOT] YMODEM failed (code: %d).\r\n", ret);
    }
}
```

### 阶段六检查点

PC 端用 Python 发送后，串口输出：

```
[BOOT] Starting YMODEM OTA...
[YMODEM] Starting receive, target=0x08040000 max=262144
[YMODEM] Sending 'C'...
[YMODEM] File: app_a.bin, Size: 5500 bytes
[YMODEM] Data transfer starting...
[YMODEM] Erasing target area 0x08040000...
[YMODEM] Progress: 1024/5500 bytes (18%)
[YMODEM] Progress: 2048/5500 bytes (37%)
...
[YMODEM] Progress: 5500/5500 bytes (100%)
[YMODEM] EOT received, total=5500 bytes
[YMODEM] Transfer complete.
[YMODEM] Reception done: 5500 bytes in 6 packets.

[BOOT] YMODEM transfer OK, verifying...
[BOOT] CRC32: 0xXXXXXXXX
[BOOT] OTA params updated. Rebooting...
```

### 阶段六常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| MCU 发 'C' 后无响应 | Python 侧未启动发送 | 确保 Python 脚本先收到第一个 'C' |
| 传输到一半超时 | 擦除 Flash 导致中断阻塞太久 | Flash 擦除操作约 1–2 秒，在此期间串口中断会积压；完成后先 `uart_flush_rx()` |
| 写入失败 | Flash 擦除后写入地址不在已擦除区域 | 确保 `flash_if_erase` 的地址和大小正确 |
| 写入失败 | 写入时 Flash 未解锁 | 在 `ymodem_receive` 中调用 `flash_if_init()` |

---

## 八、阶段七：升级状态机与回滚

> **目标**：实现完整的升级/回滚逻辑。

### 8.1 升级流程图

```
上电   →   读取 Parameter   →   检查 ota_state
                                       │
            ┌──────────────────────────┼──────────────────────────┐
            ▼                          ▼                          ▼
    ota_state = IDLE           ota_state = COMPLETE      ota_state = FAILED
    检查 active_partition      boot_count++                  │
    检查该分区栈指针/boot_count   ├─ 过大 → 回滚              ▼
    ├─ 有效 → jump_to_app      ├─ 正常 → 校验新固件     回滚到旧分区
    └─ 无效 → 进入升级模式     │   ├─ 通过 → 切分区  → jump
                              │   └─ 失败 → 回滚   → jump
                              └─ 成功 → IDLE → jump
```

### 8.2 实现

```c
// ===== 判断分区是否有效 =====
static int partition_is_valid(uint32_t addr)
{
    uint32_t sp = *((volatile uint32_t *)addr);
    uint32_t pc = *((volatile uint32_t *)(addr + 4));
    return ((sp & 0x2FFE0000) == 0x20000000);
}

// ===== 获取当前活跃分区的地址 =====
static uint32_t get_active_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_A_ADDR : APP_B_ADDR;
}

// ===== 获取非活跃分区的地址 =====
static uint32_t get_inactive_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_B_ADDR : APP_A_ADDR;
}

// ===== 切换活跃分区 =====
static void swap_active_partition(void)
{
    g_ota_param.active_partition =
        (g_ota_param.active_partition == APP_A_ACTIVE)
        ? APP_B_ACTIVE : APP_A_ACTIVE;
}

// ===== 回滚 =====
static int perform_rollback(void)
{
    printf("[BOOT] ROLLBACK triggered!\r\n");

    // 切回旧分区
    swap_active_partition();
    uint32_t old_addr = get_active_addr();

    printf("[BOOT] Rolling back to 0x%08X\r\n", (unsigned int)old_addr);

    // 检查旧分区是否有效
    if (!partition_is_valid(old_addr)) {
        printf("[BOOT] FATAL: Old partition also invalid!\r\n");
        printf("[BOOT] Entering safe mode (wait for upgrade)...\r\n");
        return -1;
    }

    // 清除升级状态
    g_ota_param.ota_state = OTA_STATE_IDLE;
    g_ota_param.boot_count = 0;
    ota_params_save(&g_ota_param);

    printf("[BOOT] Rollback OK.\r\n");
    return 0;
}

// ===== 主启动决策 =====
static int boot_decision(void)
{
    uint32_t active_addr = get_active_addr();

    switch (g_ota_param.ota_state) {

    case OTA_STATE_IDLE:
        // 正常启动
        g_ota_param.boot_count = 0;
        ota_params_save(&g_ota_param);

        if (partition_is_valid(active_addr)) {
            return 1;  // 跳转 App
        } else {
            printf("[BOOT] No valid app in active partition.\r\n");
            return 0;  // 留在 Bootloader
        }

    case OTA_STATE_COMPLETE:
        // 新固件刚写入，验证并切换
        {
            uint32_t new_addr = get_inactive_addr();

            // 增加启动计数
            g_ota_param.boot_count++;
            ota_params_save(&g_ota_param);

            printf("[BOOT] Boot attempt %d/%d\r\n",
                   g_ota_param.boot_count,
                   g_ota_param.max_boot_count);

            // 检查是否超过了最大尝试次数
            if (g_ota_param.boot_count > g_ota_param.max_boot_count) {
                printf("[BOOT] Max boot attempts exceeded, rolling back.\r\n");
                if (perform_rollback() == 0) {
                    return 1;  // 跳转到旧 App
                }
                return 0;  // 回滚失败，留在 Bootloader
            }

            // 校验新固件 CRC32
            uint32_t new_size = (new_addr == APP_A_ADDR)
                                ? g_ota_param.app_a_size
                                : g_ota_param.app_b_size;

            if (new_size == 0 || new_size > APP_B_SIZE) {
                printf("[BOOT] Invalid new firmware size, rolling back.\r\n");
                perform_rollback();
                return 1;
            }

            uint32_t calc_crc = crc32_flash(new_addr, new_size);
            uint32_t saved_crc = (new_addr == APP_A_ADDR)
                                 ? g_ota_param.app_a_crc32
                                 : g_ota_param.app_b_crc32;

            printf("[BOOT] CRC32 verify: saved=0x%08X calc=0x%08X\r\n",
                   (unsigned int)saved_crc, (unsigned int)calc_crc);

            if (calc_crc == saved_crc) {
                // 校验通过，切换分区
                printf("[BOOT] New firmware verified, switching partition.\r\n");
                swap_active_partition();
                g_ota_param.ota_state = OTA_STATE_IDLE;
                g_ota_param.boot_count = 0;
                ota_params_save(&g_ota_param);
                return 1;  // 跳转到新 App
            } else {
                printf("[BOOT] CRC32 mismatch, rolling back.\r\n");
                perform_rollback();
                return 1;
            }
        }

    case OTA_STATE_FAILED:
        // 上次升级失败，回滚
        printf("[BOOT] Previous upgrade failed, rolling back.\r\n");
        perform_rollback();
        return 1;

    default:
        printf("[BOOT] Unknown OTA state %d, entering upgrade mode.\r\n",
               g_ota_param.ota_state);
        return 0;
    }
}
```

### 8.3 最终 `main()` 主循环

```c
int main(void)
{
    uart_init();
    printf("\r\n================================\r\n");
    printf("Bootloader v1.0 (YMODEM)\r\n");
    printf("================================\r\n\r\n");

    // 1. 加载 OTA 参数
    ota_params_load(&g_ota_param);
    if (g_ota_param.magic != OTA_MAGIC) {
        printf("[BOOT] Initializing OTA params...\r\n");
        ota_params_init();
        ota_params_load(&g_ota_param);
    }

    printf("[BOOT] Active: %s, State: %d, Boot count: %d\r\n",
           g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B",
           g_ota_param.ota_state,
           g_ota_param.boot_count);

    // 2. 启动决策
    int should_jump = boot_decision();

    if (should_jump) {
        uint32_t addr = get_active_addr();
        if (partition_is_valid(addr)) {
            printf("[BOOT] Jumping to %s at 0x%08X...\r\n",
                   g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B",
                   (unsigned int)addr);
            jump_to_app(addr);
        }
    }

    // 3. 未能跳转，进入升级等待模式
    printf("[BOOT] Entering upgrade mode. Send firmware via YMODEM.\r\n");

    while (1) {
        printf("[BOOT] Waiting for YMODEM start... (press any key to trigger)\r\n");

        // 简化：检测串口输入，任意字符触发升级
        // 实际上应该检测特定命令或一直发 'C'
        uint8_t ch;
        if (uart_getc_timeout(&ch, 3000)) {
            // 收到字符，可能是 Python 端的触发信号
            printf("[BOOT] Trigger received, starting OTA.\r\n");
            ota_ymodem_start();
        } else {
            // 超时，尝试跳转
            if (partition_is_valid(get_active_addr())) {
                jump_to_app(get_active_addr());
            }
        }
    }
}
```

### 阶段七检查点

正常启动（已有 App）：

```
[BOOT] Active: App A, State: 0, Boot count: 0
[BOOT] Jumping to App A at 0x08010000...
```

升级后重启（OTA 成功）：

```
[BOOT] Active: App A, State: 3, Boot count: 0
[BOOT] Boot attempt 1/3
[BOOT] CRC32 verify: saved=0x12345678 calc=0x12345678
[BOOT] New firmware verified, switching partition.
[BOOT] Jumping to App B at 0x08040000...
```

回滚场景（CRC 不匹配）：

```
[BOOT] CRC32 verify: saved=0x12345678 calc=0xABCDEF00
[BOOT] CRC32 mismatch, rolling back.
[BOOT] ROLLBACK triggered!
[BOOT] Jumping to App A at 0x08010000...
```

### 阶段七常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| 每三次启动都会回滚 | App 启动后未将 `ota_state` 复位为 `IDLE` | App 测在正常运行后需要更新 OTA 参数（阶段八） |
| 分区切换后 App 不跑 | App 编译的目标地址不匹配 | App B 需要对应的 scatter file（或依赖编译时只编到 <= App A 地址） |

---

## 九、阶段八：App 侧工程配置

> **目标**：App 可以从 0x0801 0000 正确启动，并在运行后告知 Bootloader "启动成功"。

### 9.1 修改 App 的 `system_stm32f4xx.c`

在 `firmware/cmsis/device/system_stm32f4xx.c` 中修改 VTOR：

```c
// 将第 357 行：
#define VECT_TAB_OFFSET  0x00

// 改为（App 工程用）：
#ifdef BOOTLOADER
#define VECT_TAB_OFFSET  0x00            // Bootloader 在 0x0800 0000
#else
#define VECT_TAB_OFFSET  0x00010000      // App 在 0x0801 0000（偏移 64KB）
#endif
```

> **注意**：Keil 中 App Target 需要定义 `STM32F411xE` 和 `USE_STDPERIPH_DRIVER`，不定义 `BOOTLOADER`。

### 9.2 修改 App 的 `main.c`

App 启动后需要：

1. 验证 `ota_state` 是否正常
2. 如果状态为 `COMPLETE`（刚升级完），证明启动成功，将状态设为 `IDLE`
3. 如果状态为 `IDLE`，正常启动

```c
#include "stm32f4xx.h"
#include "usart.h"
#include <string.h>

// 引用 Bootloader 的分区定义（复制或 #include）
#define OTA_PARAM_ADDR      0x0800C000U
#define OTA_MAGIC           0x4F544152
#define OTA_STATE_IDLE      0
#define OTA_STATE_COMPLETE  3

void app_check_ota_params(void)
{
    // 直接从 Flash 读 OTA 参数（App 只有读权限）
    volatile uint32_t *p = (volatile uint32_t *)OTA_PARAM_ADDR;
    uint32_t magic = p[0];
    uint32_t ota_state_byte = (p[1] >> 8) & 0xFF;

    if (magic != OTA_MAGIC) {
        printf("[APP] OTA Params not initialized, skipping.\r\n");
        return;
    }

    printf("[APP] OTA state: %u\r\n", (unsigned int)ota_state_byte);

    if (ota_state_byte == OTA_STATE_COMPLETE) {
        printf("[APP] Upgrade confirmed! Marking as IDLE.\r\n");

        // 需要通过 Bootloader 的接口或直接写 Flash 更新
        // 简化：App 无法直接写 Flash（未链接 flash_if.c）
        // 解决方案：在 App 中也实现一个简单的 Flash 字段更新
        // 或者：通过 NVIC_SystemReset() 让 Bootloader 下次启动时识别

        // 方案：App 只做校验，Bootloader 在 IDLE 状态看到 App 合法
        //      就直接清除状态。见阶段七的 boot_decision() 中的 IDLE 处理。
        printf("[APP] Running normally.\r\n");
    }
}

int main(void)
{
    UART_Init();
    printf("\r\n================================\r\n");
    printf("App v1.0 (0x08010000)\r\n");
    printf("================================\r\n\r\n");

    // 验证向量表
    printf("[APP] SCB->VTOR = 0x%08X\r\n", (unsigned int)SCB->VTOR);

    // 检查 OTA 参数
    app_check_ota_params();

    // 主循环
    uint32_t counter = 0;
    while (1)
    {
        printf("[APP] Running... %u\r\n", (unsigned int)counter++);
        delay_ms(1000);
    }
}
```

### 9.3 重要：App B 的目标地址问题

由于 App A (0x0801 0000) 和 App B (0x0804 0000) 的链接地址不同，同一份 .bin 文件需要能够同时在两个分区运行。这需要一个关键实现：

**方案 A（推荐）：使用位置无关代码（PIC）**

在 Keil 选项 C/C++ 页中，勾选 "Read-Only Position Independent" 和 "Read-Write Position Independent"，使生成的代码可以在不同地址运行。

**方案 B：编译两份 App**

分别编译 `app_a.sct` 和 `app_b.sct`，但需要维护两个 Target。

**方案 C：统一 VTOR 偏移**

App 编译时固定 `VECT_TAB_OFFSET=0x00010000`（即假设在 App A）。Bootloader 将固件写入 App B 后，跳转前修改 SCB->VTOR 为 `APP_B_ADDR`。（但 App 中的 `SystemInit()` 会在 `SetSysClock()` 后重设 `SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET`，导致向量表指向 App A）

**解决**：修改 `system_stm32f4xx.c`，使 App 不重置 VTOR：

```c
// 在 SystemInit() 末尾：
#ifndef BOOTLOADER
    // App 模式下不覆盖 Bootloader 设置的 VTOR
    // SCB->VTOR 由 Bootloader 在跳转前设置
#else
#ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET;
#else
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
#endif
#endif
```

或者最简方案：**App 在 main() 开头自己设置 VTOR**：

```c
// App main() 开头
SCB->VTOR = APP_A_ADDR;  // 注意：需要知道编译目标
```

更好的方案：**App 使用自己的起始地址**：

```c
#define APP_BASE_ADDR   APP_A_ADDR   // App A: 0x08010000
// 或通过分散加载文件
SCB->VTOR = APP_BASE_ADDR;
```

### 阶段八检查点

App 单独烧录到 0x0801 0000 后启动：

```
================================
App v1.0 (0x08010000)
================================
[APP] SCB->VTOR = 0x08010000
[APP] OTA state: 0
[APP] Running normally.
[APP] Running... 0
[APP] Running... 1
```

### 阶段八常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| App 启动后 HardFault | VECT_TAB_OFFSET 未修改 | 确保 App 的 system_stm32f4xx.c 中设为 0x10000 |
| App 启动后死机 | scatter file 链接地址与烧录地址不一致 | Keil 的 IROM1 + scatter file 都要指向 0x0801 0000 |
| App 串口无输出 | 串口时钟在 App 中未开启 | App 的 `SystemInit()` 会重置 RCC，需要确保 USART1 时钟开启 |

---

## 十、阶段九：PC 端发送工具

> **目标**：Python 脚本通过 YMODEM 协议发送 .bin 固件。

### 10.1 安装依赖

```bash
pip install pyserial
```

### 10.2 创建 `tools/ymodem_send.py`

```python
#!/usr/bin/env python3
"""
YMODEM Sender for STM32F411 OTA Bootloader
Usage: python ymodem_send.py <COM_PORT> <BIN_FILE>
Example: python ymodem_send.py COM3 app_a.bin
"""

import serial
import sys
import os
import time
import struct

# ===== YMODEM 常量 =====
SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
C_CHAR = 0x43

PACKET_SIZE = 1024  # YMODEM-1K


def crc16(data: bytes) -> int:
    """计算 YMODEM CRC16"""
    crc = 0
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def ymodem_send(ser, filepath: str) -> bool:
    """发送文件"""
    filename = os.path.basename(filepath)
    filesize = os.path.getsize(filepath)

    print(f"[SEND] File: {filename}, Size: {filesize} bytes")

    # ===== 等待 MCU 发送 'C' =====
    print("[SEND] Waiting for 'C' from MCU...")
    ser.reset_input_buffer()
    while True:
        ch = ser.read(1)
        if ch == b'C':
            print("[SEND] Got 'C', starting transmission.")
            break
        if ch:
            sys.stdout.write(ch.decode('ascii', errors='replace'))
            sys.stdout.flush()

    # ===== 发送文件名包（序号 0） =====
    packet = bytearray(PACKET_SIZE)
    # 填 0
    for i in range(PACKET_SIZE):
        packet[i] = 0
    # 文件名
    name_bytes = filename.encode('ascii')
    for i, b in enumerate(name_bytes):
        if i < 63:
            packet[i] = b
    # 文件大小（ASCII）
    size_str = str(filesize).encode('ascii')
    offset = len(name_bytes) + 1
    for i, b in enumerate(size_str):
        if offset + i < 64:
            packet[offset + i] = b

    seq = 0
    _send_packet(ser, seq, bytes(packet))

    # 等待 ACK
    if not _wait_ack(ser):
        return False

    # ===== 发送第二个序号 0 包（确认） =====
    print("[SEND] Sending second seq=0 packet...")
    _send_packet(ser, seq, b'\x00' * PACKET_SIZE)
    if not _wait_ack_or_c(ser):
        return False

    print("[SEND] Data transfer starting...")

    # ===== 发送数据包 =====
    seq = 1
    with open(filepath, 'rb') as f:
        while True:
            data = f.read(PACKET_SIZE)
            if not data or len(data) == 0:
                break

            # 填充到 1024 字节
            if len(data) < PACKET_SIZE:
                padding = bytes(PACKET_SIZE - len(data))
                data = data + padding

            _send_packet(ser, seq, data)

            if not _wait_ack(ser):
                return False

            progress = min(seq * PACKET_SIZE, filesize)
            print(f"[SEND] Progress: {progress}/{filesize} "
                  f"({100 * progress // filesize}%)")

            seq = (seq + 1) & 0xFF

    # ===== 发送 EOT =====
    print("[SEND] Sending EOT...")
    ser.write(bytes([EOT]))
    ser.flush()

    # 等待 NAK
    ch = ser.read(1)
    if ch[0] != NAK:
        print(f"[SEND] Expected NAK, got 0x{ch[0]:02X}")
        return False

    # 发送第二个 EOT
    print("[SEND] Sending second EOT...")
    ser.write(bytes([EOT]))
    ser.flush()

    if not _wait_ack(ser):
        return False

    # ===== 发送空文件名包（结束传输） =====
    print("[SEND] Sending end-of-transmission packet...")
    _send_packet(ser, 0, b'\x00' * PACKET_SIZE)

    if not _wait_ack(ser):
        return False

    print("[SEND] Transfer complete!")
    return True


def _send_packet(ser, seq: int, data: bytes):
    """发送一个 YMODEM 数据包"""
    pkt = bytearray()
    pkt.append(STX)           # 1024 字节包标记
    pkt.append(seq & 0xFF)    # 序号
    pkt.append((~seq) & 0xFF) # 序号反码
    pkt.extend(data)          # 数据

    crc = crc16(data)
    pkt.append((crc >> 8) & 0xFF)
    pkt.append(crc & 0xFF)

    ser.write(bytes(pkt))
    ser.flush()


def _wait_ack(ser, timeout=5) -> bool:
    """等待 ACK"""
    timeout = 10  # 10 秒超时
    start = time.time()
    while time.time() - start < timeout:
        if ser.in_waiting:
            ch = ser.read(1)
            if ch[0] == ACK:
                return True
            elif ch[0] == NAK:
                print("[SEND] Got NAK!")
                return False
            elif ch[0] == CAN:
                print("[SEND] Got CAN (cancel)!")
                return False
    print("[SEND] ACK timeout!")
    return False


def _wait_ack_or_c(ser, timeout=5) -> bool:
    """等待 ACK 或 'C'（YMODEM 第二步也用 'C' 应答）"""
    start = time.time()
    while time.time() - start < timeout:
        if ser.in_waiting:
            ch = ser.read(1)
            if ch[0] in (ACK, C_CHAR):
                return True
            if ch[0] == NAK:
                print("[SEND] Got NAK!")
                return False
    print("[SEND] ACK/C timeout!")
    return False


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <COM_PORT> <BIN_FILE>")
        print(f"Example: {sys.argv[0]} COM3 firmware.bin")
        sys.exit(1)

    port = sys.argv[1]
    filepath = sys.argv[2]

    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        sys.exit(1)

    print(f"Opening serial port {port}...")
    try:
        ser = serial.Serial(
            port=port,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1.0
        )
    except serial.SerialException as e:
        print(f"Error opening {port}: {e}")
        sys.exit(1)

    # 重置 DTR（部分板子会触发复位）
    ser.dtr = False
    time.sleep(0.5)
    ser.dtr = True

    success = ymodem_send(ser, filepath)
    ser.close()

    if success:
        print("=" * 40)
        print("  SUCCESS: Firmware sent!")
        print("=" * 40)
    else:
        print("=" * 40)
        print("  FAILED: Transfer failed!")
        print("=" * 40)
        sys.exit(1)


if __name__ == '__main__':
    main()
```

### 10.3 使用方法

```bash
# 1. 编译 App Target，生成 firmware.bin
# 2. Bootloader 已经运行在 MCU 上，串口已连接
# 3. 执行：
python tools/ymodem_send.py COM3 firmware.bin
```

### 阶段九检查点

Python 脚本正常运行时：

```
Opening serial port COM3...
[SEND] File: app_a.bin, Size: 5500 bytes
[SEND] Waiting for 'C' from MCU...
[BOOT] Bootloader v1.0...
[SEND] Got 'C', starting transmission.
[SEND] Sending second seq=0 packet...
[SEND] Data transfer starting...
[SEND] Progress: 1024/5500 (18%)
[SEND] Progress: 2048/5500 (37%)
...
[SEND] Sending EOT...
[SEND] Sending second EOT...
[SEND] Sending end-of-transmission packet...
[SEND] Transfer complete!
========================================
  SUCCESS: Firmware sent!
========================================
```

---

## 十一、阶段十：联调与验证

### 11.1 测试场景

| 编号 | 场景 | 操作 | 期望结果 |
|------|------|------|---------|
| T1 | 首次烧录 Bootloader | ST-Link 烧录 boot_main.hex | Bootloader 启动，打印信息，因无有效 App 停留在升级模式 |
| T2 | 首次 OTA 升级 | Python 发送 App bin | 传输成功，Bootloader 写入 App B，切换分区，跳转 |
| T3 | App 正常运行 | 复位后 | Bootloader 识别 App 有效，直接跳转 |
| T4 | 第二次 OTA | Python 发送新 App bin | 写入非活跃分区，切换，运行新版本 |
| T5 | 回滚测试 | 修改 App bin 中一个字节 | 发送后 CRC 不匹配，Bootloader 回滚到旧版本 |
| T6 | 断线恢复 | 传输中断开串口线 | 超时后 Bootloader 打印错误，回到等待状态 |
| T7 | 看门狗 | Bootloader 中打开 IWDG | 确保升级过程中持续喂狗，不意外复位 |

### 11.2 完整操作流程

```
1. 编译 Bootloader Target → 生成 bootloader.hex
2. ST-Link 烧录 bootloader.hex 到 0x0800 0000
3. 上电，打开串口助手（115200），观察 Bootloader 打印
4. 编译 App Target → 生成 app.hex → 用 fromelf 转 app.bin:
   fromelf --bin --output app_a.bin app.axf
5. 关闭串口助手
6. 运行: python tools/ymodem_send.py COM3 app_a.bin
7. 观察传输进度和结果
8. 重新打开串口助手，观察 App 启动打印
9. 重复步骤 4-8 测试 OTA 升级和回滚
```

---

## 附录 A：Flash Sector 编号表 (F411)

| Sector # | 地址范围 | 大小 | `FLASH_Sector_n` 常量 |
|----------|----------|------|----------------------|
| 0 | 0x0800 0000 – 0x0800 3FFF | 16KB | `FLASH_Sector_0` (0x00) |
| 1 | 0x0800 4000 – 0x0800 7FFF | 16KB | `FLASH_Sector_1` (0x08) |
| 2 | 0x0800 8000 – 0x0800 BFFF | 16KB | `FLASH_Sector_2` (0x10) |
| 3 | 0x0800 C000 – 0x0800 FFFF | 16KB | `FLASH_Sector_3` (0x18) |
| 4 | 0x0801 0000 – 0x0801 FFFF | 64KB | `FLASH_Sector_4` (0x20) |
| 5 | 0x0802 0000 – 0x0803 FFFF | 128KB | `FLASH_Sector_5` (0x28) |
| 6 | 0x0804 0000 – 0x0805 FFFF | 128KB | `FLASH_Sector_6` (0x30) |
| 7 | 0x0806 0000 – 0x0807 FFFF | 128KB | `FLASH_Sector_7` (0x38) |

> **注意**：`FLASH_Sector_n` 的值不是 `n`，而是 `n * 8`！
> 例如 `FLASH_Sector_3 = 0x18 = 24 = 3 * 8`。

---

## 附录 B：常见 HardFault 排查

OTA 开发中最常见的 HardFault 原因及排查方法：

| 序号 | 原因 | 排查方式 | 解决 |
|------|------|---------|------|
| 1 | App 未设置 VTOR | HardFault 发生在 SysTick/PendSV 中断 | 确保 `SCB->VTOR` 指向 App 分区地址 |
| 2 | 跳转前外设未关闭 | 跳转后进入某些中断处理函数 | `jump_to_app()` 中禁用所有中断并清除挂起 |
| 3 | 栈指针非法 | `jump_to_app()` 打印 SP 不在 RAM 范围 | 检查 App 的向量表是否有效 |
| 4 | Flash 擦写过程中执行了 Flash 中的代码 | 擦除/写入操作卡死 | 确保擦写目标不是当前执行的区域 |
| 5 | 写入地址未对齐 | `FLASH_ProgramWord` 返回错误 | Flash 写入地址必须 4 字节对齐 |
| 6 | 写入了未擦除的 Flash | 写入后读回值不对 | Flash 写之前必须先擦除 |
| 7 | 看门狗超时 | 擦除大扇区时触发 WDG 复位 | 在擦除循环中添加喂狗操作 |

### HardFault 调试技巧

在 Keil Debug 模式下，HardFault 发生时查看：

```
SCB->HFSR   — HardFault 状态寄存器
SCB->CFSR   — 可配置故障状态寄存器（含 MMFAR/BFAR 有效标志）
SCB->MMFAR  — MemManage 故障地址
SCB->BFAR   — BusFault 故障地址
LR          — 链接寄存器（判断异常前状态：ARM/Thumb、MSP/PSP）
```

---

## 关键提醒

1. **Flash 扇区擦除必须在扇区边界对齐**，STM32F4 擦除粒度为 Sector（16/64/128KB）。
2. **Flash 写入按 Word（4 字节）**，写入前必须擦除（全 0xFF）。
3. **VTOR 是 OTA 最容易出错的地方**，App 必须在启动时设置正确的向量表地址。
4. **串口接收/发送不能与 Flash 擦写冲突**，Flash 擦写期间要从 RAM 执行代码。
5. **优先级问题**：Bootloader 中不要使用 SysTick 中断（使用轮询延时），避免在 Flash 操作期间触发中断。
6. **调试时先用 ST-Link 烧录 Bootloader，再用串口 OTA App**，不要试图用 ST-Link 同时烧录两份（ST-Link 会擦掉所有扇区）。
