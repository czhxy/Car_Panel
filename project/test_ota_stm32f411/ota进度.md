# OTA 实现进度

> 最后更新：2026-06-14 22:30
> 文件用途：AI 读取此文件即可了解当前进度、下一步任务和已知问题

---

## 总体进度

```
阶段一  [████████] 工程配置
阶段二  [████████] Bootloader 骨架
阶段三  [░░░░░░░░] Flash 操作层        ← 下一步
阶段四  [░░░░░░░░] CRC32 校验
阶段五  [░░░░░░░░] OTA Parameter 管理
阶段六  [░░░░░░░░] YMODEM 协议实现
阶段七  [░░░░░░░░] 升级状态机与回滚
阶段八  [░░░░░░░░] App 侧工程配置
阶段九  [░░░░░░░░] PC 端发送工具
阶段十  [░░░░░░░░] 联调与验证

已完成：2/10    当前：阶段三
```

---

## 已完成详情

### 阶段一：工程配置 ✅

- **`boot_config.h`** — 分区地址宏定义（48KB Bootloader / 16KB Parameter / 192KB App A / 256KB App B）
- **`mdk/bootloader.sct`** — Bootloader 分散加载（0x08000000, 48KB）
- **`mdk/app.sct`** — App A 分散加载（0x08010000, 192KB）
- **`mdk/app.uvprojx`** — App 工程（Define: `STM32F411xE,...`，无 BOOTLOADER 宏）
- **`mdk/bootloader.uvprojx`** — Bootloader 工程（Define: 含 `BOOTLOADER` 宏）
- **`mdk/workspace.uvmpw`** — 多工程工作区（Keil 自动生成格式）
- **`system_stm32f4xx.c`** — VECT_TAB_OFFSET 按 BOOTLOADER 宏区分（0x00 vs 0x10000）

### 阶段二：Bootloader 骨架 ✅

- **`bootloader/boot_main.c`** — 包含：
  - `jump_to_app()` — 栈指针校验 + 中断关闭 + VTOR 设置 + MSP 跳转
  - `delay_ms()` — SysTick 轮询延时（不使用中断）
  - `partition_is_valid()` — 检查 App 分区栈指针合法性
- **编译通过 ✓**
- **无 App 时串口测试 ✓**（115200, PA9/PA10）：
  ```
  ================================
  Bootloader v1.0 (STM32F411)
  Flash: 512KB (0x08000000 - 0x0807FFFF)
  ================================
  [BOOT] App A @ 0x08010000: PC=0xFFFFFFFF
  [BOOT] App A invalid, staying in bootloader.
  [BOOT] Waiting for YMODEM upgrade...
  ```
- **Bootloader→App 跳转链路验证通过 ✓**：
  ```
  [BOOT] App A looks valid, jumping...
  [BOOT] Jumping to App at 0x08010000...
  [BOOT]   SP = 0x20000418, PC = 0x0801019D
  [APP] VTOR=0x08010000  ← 确认向量表正确指向 App A
  hello
  ```
- **App VTOR 验证 ✓**：`SCB->VTOR = 0x08010000`，中断向量表正确

---

## 当前任务：阶段三 — Flash 操作层

### 需要编写的文件

| 文件 | 状态 | 内容 |
|------|------|------|
| `bootloader/flash_control.h` | 🔲 空文件 | 函数声明 |
| `bootloader/flash_control.c` | 🔲 空文件 | 扇区擦除 + 字写入实现 |

### 核心 API

```c
void flash_if_init(void);      // 解锁 Flash
void flash_if_lock(void);      // 锁定 Flash
int  flash_if_erase(uint32_t addr, uint32_t size);   // 按扇区擦除
int  flash_if_write(uint32_t addr, uint8_t *data, uint32_t len); // 批量写入
```

### 关键提醒

1. **STM32F4 Flash 擦除单位是 Sector**，不能部分擦除——这就是分区必须扇区对齐的原因
2. **写入前必须擦除**，Flash 只能 1→0（写），不能 0→1（擦除后才是全 1）
3. **写入单位是 Word（4 字节）**，用 `FLASH_ProgramWord()`
4. **擦写期间不能访问被操作的 Flash bank**，但 F411 只有一个 bank，所以擦写期间代码必须从 RAM 执行，或者确认擦写的扇区不是当前代码所在的扇区
5. `VoltageRange_3` 对应 Scale1 模式（2.7-3.6V），与 `SystemInit()` 中的 `PWR_CR_VOS` 匹配

### 实现提示

- `stm32f4xx_flash.h` 中 `FLASH_EraseSector()` 的第一个参数是 `FLASH_Sector_n`（值是 `n*8`，不是 n）
- Sector 0-3 各 16KB，Sector 4 是 64KB，Sector 5-7 各 128KB
- `FLASH_Unlock()` / `FLASH_Lock()` 成对使用

### 验证方式

写测试函数擦除 OTA Parameter 区（Sector 3），写入测试数据，回读验证：

```
[TEST] Flash operations test...
[TEST] Erase OK.
[TEST] Write OK.
[TEST] Readback: 0x4F544152 0x00010000 ...
[TEST] Flash test DONE.
```

---

## 文件清单

### 待实现的空文件（骨架已建）

```
bootloader/
├── flash_control.h    ← 阶段三（当前）
├── flash_control.c    ← 阶段三（当前）
├── ota_params.h       ← 阶段四+五
├── ota_params.c       ← 阶段四+五
├── ymodem.h           ← 阶段六
├── ymodem.c           ← 阶段六
```

### 已完成文件

```
app/
└── main.c             ✅（最小 App，验证跳转）

bootloader/
├── boot_config.h      ✅
└── boot_main.c        ✅（阶段一+二）

mdk/
├── workspace.uvmpw    ✅
├── app.uvprojx        ✅
├── app.sct            ✅
├── bootloader.uvprojx ✅
└── bootloader.sct     ✅

firmware/cmsis/device/
└── system_stm32f4xx.c ✅（VECT_TAB_OFFSET 已修改）
```

---

## 问题记录

| 编号 | 问题 | 状态 |
|------|------|------|
| #1 | Workspace 文件格式不兼容，Keil 自动重建 | 已解决（Keil 自动生成正确格式） |
| #2 | `fputc` 多重定义（boot_main.c 和 usart.c） | 已解决（删掉 boot_main.c 中的重复定义） |
| #3 | 旧工程文件残留（stm32f411.uvprojx） | 已解决（已删除） |
| #4 | `partition_is_valid()` 未调用 | 已解决（main() 中改用函数调用替代内联判断） |
| #5 | Bootloader→App 跳转后疑似复位 | 已解决（实为手动复位，跳转链路正常） |
