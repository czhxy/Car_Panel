# CLAUDE.md — Car_Panel 汽车双 ECU 仪表盘项目

> 给 AI 助手的总体概览。各子项目的详细进度见各自的 `HANDOFF.md`。

## 项目简介

双 MCU 汽车仪表盘系统：

| ECU | MCU | 职责 | 当前状态 |
|---|---|---|---|
| 显示域 | STM32F429IGT6 @ 180MHz | Bootloader/YMODEM OTA、LTDC LCD、LVGL UI、CAN 通信 | CAN 通信完整，LCD/UI 待开发 |
| 动力域 | STM32F103C8T6 @ 72MHz | 编码器测速、PWM 电机控制、PID 闭环、CAN 上报 | CAN 基础设施完成，电机驱动/协议帧待实现 |

通信：CAN 500kbps / 29-bit 扩展帧。

**各 ECU 概览与进度：**
- 显示域 → `project/display_ecu_f429/CLAUDE.md` + `HANDOFF.md`
- 动力域 → `project/power_ecu_f103/CLAUDE.md` + `HANDOFF.md`

## 当前分支

- **分支**：`feature/motor_can`

## Flash 分区（F429 片上 2MB，用 1MB）

```
0x08000000 - 0x0800FFFF   Bootloader       64 KB   Sector 0-3
0x08010000 - 0x0801FFFF   OTA 参数区       64 KB   Sector 4 (append-only 日志)
0x08020000 - 0x0807FFFF   App A (活跃槽)   384 KB  Sector 5-7
0x08080000 - 0x080DFFFF   App B (备用槽)   384 KB  Sector 8-10
0x080E0000 - 0x080FFFFF   (预留)           128 KB  Sector 11
```

## RAM 区划

```
0x20000000 - 0x2001FFFF   主 SRAM 128KB    DMA 缓冲/全局/栈
0x10000000 - 0x1000FFFF   CCM 64KB        FreeRTOS 堆 (heap_4 ucHeap)
```

## 关键技术约束

### CCM 与 DMA
- FreeRTOS 堆位于 CCM（CPU 独占总线），**DMA 控制器无法访问 CCM**
- 任何 DMA 缓冲必须静态分配（`.bss` 或全局数组，落在主 SRAM `0x2000xxxx`）
- 禁止对 `pvPortMalloc` 返回的指针进行 DMA 操作

### 真 AB 分区机制
- 每个槽独立链接一份镜像（通过各自的 `.sct` scatter 文件），槽 A 链接到 `0x08020000`，槽 B 链接到 `0x08080000`
- Bootloader 根据 `active_partition` 设 `SCB->VTOR` 后跳转
- 升级：YMODEM 写**非活跃槽** → 校验 reset-handler 地址（防错包）→ 翻转 active → 重启
- 回滚：CRC 失败切活跃槽标记，零 Flash 搬运
- `boot_decision.c` 是核心：负责 CRC 校验 + 选址 + 回滚决策

### OTA 参数区（Sector 4）
- Append-only 磨损均衡日志，1024 槽 × 64B
- 每条记录含 CRC32，常规 save 无需擦除（直接追加）
- 写一半掉电：该槽 CRC 不匹配，Bootloader 跳过并取上一条有效记录
- 满 1024 槽时整擦重写（频率≈1/1024）

### CAN 发送架构
```
应用层 → ModCanFrame → Mod_Can_TxEvent() → TX 队列
                                            ↓
                                     ModCommCan_Tx() 出队
                                            ↓
                                  CanTxMsg → CAN_Transmit()
```
- `ModCommCan_Tx()` 非阻塞消费，邮箱满则**回灌队首** break
- RX 路径：CAN FIFO0 中断 → `Mod_Can_RxIRQHandler()` → RX 队列 → `Mod_Can_RxTask()` → 弱符号 `ModCommCan_OnRxFrame()`

## 工程结构

```
Car_Panel/
  CLAUDE.md              ← 本文件（总体概览）
  docs/                  设计文档
  project/
    display_ecu_f429/    显示域 ECU（STM32F429, FreeRTOS, 当前主力开发）
    power_ecu_f103/      动力域 ECU（STM32F103, 裸机, CAN 基础设施已完成）
```

## 编译要点

- **编译器**：armcc V5.06（ARM Compiler 5）
- **C 标准**：C99
- app 和 boot 是**独立工程**，手动切换编译
- 显示域 app 工程有两个 Target：`stm32f429`（A 槽）和 `stm32f429_b`（B 槽）
- 动力域单 Target

## 编码规范

- BSP 层：`app/bsp_<module>.c/h` — 硬件抽象
- 任务层：`task/mod_<module>.c/h` 或 `task/task_<module>.c/h` — 业务逻辑
- 日志：`LOG_E`/`LOG_W`/`LOG_I`/`LOG_D`（`bsp_log.h`）
- CAN 协议 ID 位域集中定义在 `protocol/CAN_Protocol.h`
- 注释和文档用中文，UTF-8 编码

## 参考文档

- 完整方案：`docs/Car_Panel_F429_Project_Plan.md`
- 真 AB 分区设计：`project/display_ecu_f429/ab_partition_design.md`
- CCM/DMA 规范：`project/display_ecu_f429/ccm_dma_guideline.md`
- 变更记录：`project/display_ecu_f429/change_*.md`
