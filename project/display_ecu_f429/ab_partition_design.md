# 真 AB 分区设计思路 —— display_ecu_f429

> 日期：2026-07-02　MCU：STM32F429IGTx　工具链：Keil MDK / **armcc V5.06** / SPL + FreeRTOS
> 配套：落地变更见 `change_2026-07-02.md`（阶段 B）；评审依据见 `review_2026-07-02.md`（阶段三）。

本文档只讲**为什么这么设计**与**关键机制**，不复述逐行改动。

---

## 1. 背景与目标

原工程虽然预留了 A、B 两个等大分区，但实际是"A 区固定运行、B 区固定备份"：App 永远链接在 `0x08020000`、`get_active_addr()` 恒返回 A、OTA 烧完 A 才算数、回滚靠把 B **整片复制回 A**。这不是真 AB，代价是：无法瞬时切换、回滚要擦+拷 384KB（数秒、磨损、掉电风险）、B 区 384KB 长期只当仓库。

目标：实现**真 A/B**——两槽各跑各的、OTA 写入非活跃槽后瞬时切换、失败瞬时回滚、零 384KB 搬运。

---

## 2. 先纠正一个广泛流传的错误结论

> "armcc5 下不能灵活重定向向量表，所以做不了真 AB。" —— **错。**

这个说法把两件本不相干的事混成了一件：

| 需求 | 由谁实现 | armcc5 |
| --- | --- | --- |
| **向量表重定向到某地址** | `SCB->VTOR`（Cortex-M4 的 **CPU 寄存器**） | ✅ 与编译器无关 |
| **同一份 .bin 跑在两个不同地址** | 位置无关代码（PIC / ROPI / RWPI） | ❌ Cortex-M 不可用 |
| **两份各自链接的镜像、各跑各的** | 链接地址不同 + VTOR | ✅ |

**反证就在本工程里**：`boot_jump.c` 的 `jump_to_app()` 早就在写 `SCB->VTOR = app_addr & 0xFFFFFF00;`，App 的 `main()` 也再设 `SCB->VTOR = &__Vectors;`——armcc5 下能跑能跳，本身就是 VTOR 重定向在工作。

所以真 AB 的真正约束**从来不是向量表**，而是：**能否让一份二进制在两个地址都正确执行**。这需要位置无关代码，armcc5 对 Cortex-M 基本不支持（armclang/AC6 才有 `-fropi/-frwpi`）。

**但真 AB 并不强求"一份通用二进制"。** 工业主流做法是"每个槽位各自链接一份镜像"——这条路 armcc5 完全可走，本设计即采用此方案。

---

## 3. 总体架构

```
                ┌─────────────── 0x08000000 ────────────────┐
   复位入口 ───▶ │ Bootloader 64KB (Sector 0-3)              │
                │   boot_decision() 按 active_partition 选址 │
                └───────────────────────────────────────────┘
                          │ jump_to_app(active_addr)
                          │   设 MSP、SCB->VTOR=active_addr、跳转
              ┌───────────┴────────────────┐
              ▼                            ▼
   ┌── 0x08020000 ──┐            ┌── 0x08080000 ──┐
   │ App A 384KB    │            │ App B 384KB    │
   │ (Sector 5-7)   │            │ (Sector 8-10)  │
   │ 链接@0x08020000│            │ 链接@0x08080000│
   └────────────────┘            └────────────────┘
              │                            │
              └────────── 同一份源码 ───────┘
                   (app.uvprojx 的两个 Target: stm32f429 / stm32f429_b)

   参数区 0x08010000 (Sector 4, 64KB)：append-only 日志，记录 active_partition
          + A/B 各自 version/size/crc + ota_state + boot_count
```

- **A、B 是两份独立链接的镜像**，源码完全相同，仅 scatter 链接地址与 `VECT_TAB_OFFSET` 不同。
- 任一时刻只跑一份，**RAM 布局相同**（128KB SRAM + 64KB CCM），互不干扰。
- 哪个槽"活跃"由参数区的 `active_partition` 决定，bootloader 据此选址跳转。

---

## 4. 关键机制

### 4.1 向量表怎么对（VTOR，不是 PIC）

每个镜像的向量表被 scatter 放在该槽的起始地址（`*.o (RESET, +First)`），表里的 Reset_Handler 地址被链接器填成**本槽内的正确绝对地址**。因此：

- A 镜像向量表在 `0x08020000`，Reset_Handler 字段指向 `0x0802xxxx`；
- B 镜像向量表在 `0x08080000`，Reset_Handler 字段指向 `0x0808xxxx`。

跳转时 `jump_to_app()` 把 `SCB->VTOR` 设成该槽基址，CPU 就从该槽的向量表取中断入口。**无需 PIC**——绝对地址在链接时已正确固化。

> App `main()` 还会再用 `&__Vectors` 设一次 VTOR（`&__Vectors` 是链接符号，A/B 各自解析到本槽基址），属于双保险。boot_jump 跳转到 main 之间中断是关闭的（`__disable_irq` 后未开，直到 main 里 `__enable_irq`），所以即便 SystemInit 早期 VTOR 短暂为别值也不会被任何中断使用。B 目标额外定义 `APP_SLOT_B` 让 SystemInit 的 `VECT_TAB_OFFSET=0x00080000`，连这层理论窗口也消除了。

### 4.2 启动决策（boot_decision 状态机）

```
读参数区 →
├─ IDLE：partition_is_valid(活跃槽 SP) ？ 跳活跃槽 : 进 OTA
├─ COMPLETE（OTA 刚翻转完 active）：
│      boot_count++
│      CRC32 校验【活跃槽】(用其 size/crc)
│      ├─ 通过 → IDLE + 清计数 → 跳活跃槽
│      ├─ 失败 & 计数<max → NVIC_SystemReset() 重启重试
│      └─ 失败 & 计数>=max → rollback_to_other()：校验另一槽 → 切回 → 跳
│             另一槽也坏 → 进 OTA
├─ FAILED / 未知 → 进 OTA
```

要点：
- **CRC 失败先重启重试**（不是立即回滚），给偶发错误 N 次机会；超 `max_boot_count`（默认 3）才回滚。`boot_count` 在重启前已持久化，重启后递增。
- **回滚 = 切槽**，不是搬数据。另一槽里仍完整保留着上一次的好固件。

### 4.3 OTA 流程（写非活跃槽 + 翻转 active）

```
进 OTA（按键或决策失败）：
   target = 与 active 相反的槽
   YMODEM 接收 → 直接写入 target（擦 target 扇区 + 烧录）
   校验镜像 reset-handler(偏移 +4) 落在 target 地址范围内（防错包）
   CRC32 校验 target
   记录 target 的 version/size/crc（旧槽元数据不动）
   active_partition = target；ota_state = COMPLETE；boot_count = 0
   持久化 → NVIC_SystemReset()
```

- 写非活跃槽**不影响正在运行的活跃槽**——即便写到一半掉电，重启后仍能跑旧槽。
- **防错包**：位置链接镜像的 reset-handler(向量表偏移 +4) 必落在自身槽地址范围内。bootloader 写完后校验此项；若上位机发错了 bin（如活跃 A 时发 `app.bin` 而非 `app_b.bin`，reset-handler 指向 A 区）→ **直接中止、不翻转 active、不重启**，活跃槽毫发无损、串口报错回到重试循环。CRC 拦不住这种"链接地址错配"（存验同表仍会过），这层 reset-handler 校验才是防线。
- 翻转 `active` 是一次"小写入"（参数区一条记录），配合 4.4 的原子化，掉电也安全。
- 新固件是否真好，**留到重启后由 boot_decision 的 COMPLETE 路径验证**（CRC + 试运行计数）。

### 4.4 参数区原子性（掉电安全的根）

参数正确性是真 AB 的命脉：`active_partition` 翻错了，就不知道该跑谁。原实现每次 save 都**整擦 64KB 扇区再写 64B**——掉电落在擦除与写之间，magic 丢失，参数全清，回滚信息尽失。

改为 **append-only 磨损均衡日志**（Sector 4，64KB）：
- 每槽 64B，共 1024 槽；每条记录 = `magic + seq + ota_param_t + crc32`。
- `save()` 把新记录写到下一个**已擦除槽**（无需擦除）；写一半掉电 → 该记录 CRC 不匹配 → `load()` 跳过它、取上一条有效记录。**常规 save 完全掉电安全。**
- 仅当 1024 槽写满（约每 1024 次 save）才整扇区擦除重写槽 0；该低频事件（1/1024）的窗口若掉电，退化为"参数初始化、active 默认回 A"——仍能安全启动（A 是上一次的好固件）。
- 附带红利：闪存寿命从"每 save 一次擦除"降到"每 1024 save 一次擦除"。

> CRC32 表在本次修正了 3 处历史笔误（`0x4A/0x5B/0xBB`），现为标准 IEEE 802.3 CRC-32，可被上位机标准工具互验。

### 4.5 回滚（切槽，零搬运）

`rollback_to_other()`：校验另一槽——
- 有元数据（size/crc）→ 严格 CRC 校验；
- 无元数据（如出厂仅烧了 A、B 从未 OTA）→ 仅凭 SP 合法性信任；
- 通过则把 `active_partition` 翻回另一槽，下次启动即跑旧固件。

**没有 384KB 的擦/拷**：瞬时、零磨损、零掉电风险放大。

---

## 5. 构建与交付（两份镜像）

| 目标 | scatter | 链接地址 | 工程文件 | 产物 |
| --- | --- | --- | --- | --- |
| Bootloader | `bootloader.sct` | 0x08000000 | `boot.uvprojx` | `boot.axf` |
| App A | `app.sct` | 0x08020000 | `app.uvprojx` · Target `stm32f429` | `app.axf` / `app.bin` |
| App B | `app_b.sct` | 0x08080000 | `app.uvprojx` · Target `stm32f429_b`（定义 `APP_SLOT_B`） | `app_b.axf` / `app_b.bin` |

- App A、B **同一份源码、同一份工程文件**（`app.uvprojx` 的两个 Target），仅链接地址/输出不同；新增 `.c/.h` 只加一次。命令行用 `UV4 -r app.uvprojx -t stm32f429` / `-t stm32f429_b` 分别构建（见变更说明 3.6）。
- **上位机 OTA 需按"目标槽=非活跃槽"下发对应链接的镜像**：当前活跃 A 时，下发的应是 `app_b.bin`；反之发 `app.bin`。上位机可经芯片信息查询（`task_query` 的 `0xAA 0x55 0x01`，应答里 `partition` 由 `SCB->VTOR` 自证）得知当前运行槽，据此选包。
- 三个工程输出名/产物已分离（`boot`/`app`/`app_b`），不再互相覆盖。

---

## 6. 与替代方案对比

| 方案 | 编译器 | 瞬时切换/回滚 | 单一通用 bin | 改动量 | 风险 | 选用 |
| --- | --- | --- | --- | --- | --- | --- |
| 原"A 运行 B 备份、回滚靠搬" | armcc5 ✅ | ❌（要拷 384KB） | — | — | 低 | 否（已替换） |
| **方案① 双槽各自链接（本设计）** | armcc5 ✅ | ✅ | ❌（A/B 各一份） | 中 | 低 | ✅ |
| 方案② 单一通用 bin（ROPI/RWPI） | 需 armclang(AC6) | ✅ | ✅ | 大 | 中高 | 否（暂不迁移编译器） |

本设计选①：在 armcc5 内可做、复用现有 VTOR 跳转、改动可控、瞬时切换+回滚俱全。唯一代价是交付两份镜像（A/B 各一），由上位机按非活跃槽选包——可接受。

---

## 7. 边界、限制与后续

- **首次 OTA 的回滚目标**：若设备出厂只烧了 A、B 从未写过，第一次 OTA 写 B、翻到 B 后若失败回滚到 A，A 此时无元数据（size/crc=0），回滚用"仅 SP 合法性信任"放行。一旦 A 经历过一次 OTA（被当作非活跃槽写过），就有了完整元数据，后续回滚走严格 CRC。建议出厂烧录时也把 A 的 size/crc 写入参数区，回滚更严谨。
- **参数区日志满（1/1024）的擦除窗口**：见 4.4，退化为安全默认。若要彻底闭合该窗口，可启用 Spare 的 Sector 11（128KB）做第二参数区、双区 ping-pong（任一时刻有效记录总在另一区），代价是多占一个扇区。当前不必要。
- **上位机按槽选包**（方案① 相对"单一通用 bin"的唯一运维差异）：活跃 A 时发 `app_b.bin`、活跃 B 时发 `app.bin`，经芯片信息查询得知当前槽后选包。即便发错，bootloader 的**防错包**（reset-handler 地址校验，见 4.3）会拒绝写入、保护活跃槽，不会砖机——只是本次 OTA 失败需重发正确包。若未来希望一份 bin 通刷两槽，可迁移 armclang 启用 ROPI/RWPI（方案②），届时可去掉 A/B 双 Target。
- **App 侧槽位自证**：App 不读参数区判断"我在哪"，而用 `SCB->VTOR` 自证（`task_query` 上报分区即据此），避免 App 与 bootloader 参数格式耦合。
- **CAN Bus-Off 已开 ABOM 自动恢复**（M5），配合车载场景。
