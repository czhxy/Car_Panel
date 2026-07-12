# CLAUDE.md — Car_Panel 汽车双 ECU 仪表盘项目

> 给 AI 助手的上下文指引。每次新对话开始时先阅读本文件。

## 项目简介

双 MCU 汽车仪表盘系统：

| ECU | MCU | 职责 |
|---|---|---|
| 显示域 | STM32F429IGT6 @ 180MHz | Bootloader/YMODEM OTA、LTDC LCD、LVGL UI、CAN 通信 |
| 动力域 | STM32F103C8T6 @ 72MHz | 编码器测速、PWM 电机控制、PID 闭环、CAN 上报 |

通信：CAN 500kbps / 29-bit 扩展帧。

## 当前分支与进度

- **分支**：`feature/motor_can`
- **主要工作集中在 `project/display_ecu_f429/`**，power_ecu_f103 仅有骨架。

### 已完成的模块

| 模块 | 文件/位置 | 备注 |
|---|---|---|
| Bootloader + 真 AB 分区 | `bootloader/boot_main.c`, `boot_decision.c`, `ota.c`, `ota_params.c` | 真 AB：每个槽独立链接镜像，回滚=切 VTOR，零 Flash 搬运 |
| OTA 参数区 | `bootloader/ota_params.c` | Sector 4，append-only 日志（1024 槽 × 64B），CRC32 保护，掉电安全 |
| FreeRTOS v11.3.0 | `third_lib/FreeRTOS` | heap_4，64KB 堆位于 CCM (`0x10000000`) |
| CAN 收发框架 | `task/mod_comm_can.c`, `app/bsp_can.c` | TX/RX 双队列，发送/推送分离架构，弱符号接收回调 |
| CAN 协议层 | `task/task_comm_can_protocol.c/h` | 29-bit ID 位域拆解、电机控制/状态帧编码 |
| 心跳帧 | `task/mod_comm_can.c:Can_Heartbeat()` | 4 字节计数 + 设备状态 |
| 按键驱动 | `app/bsp_key.c` | 中断 + FreeRTOS 信号量 |
| 串口日志 | `app/bsp_log.h`, `driver/usart.c` | `LOG_E/W/I/D` 统一宏，`vsnprintf` 安全 |
| 编译工程 | `mdk/app.uvprojx`（双 Target：stm32f429/stm32f429_b）, `mdk/boot.uvprojx` | boot 0E0W, app 0E1W (port.c 警告) |

### 未完成 / 待开发

| 任务 | 位置 | 说明 |
|---|---|---|
| **LTDC RGB LCD 驱动** | `app/bsp_lcd.c`（待建） | F429 板载 TFT，ILITEK 控制 IC，RGB 565 接口 |
| **LVGL 仪表盘 UI** | `task/task_display.c`（待建） | 仪表盘界面渲染，依赖 LTDC + DMA2D |
| **动力域 ECU 业务逻辑** | `project/power_ecu_f103/task/` | `task_comm_can.c`、`task_motor_ctl.c` 全部为空文件，需要实现 CAN 收发、编码器测速、PWM 控制、PID 闭环 |
| **动力域 ECU 驱动** | `project/power_ecu_f103/driver/` | `drv_can.c`、`drv_usart.c` 均为空框架 |
| **W25Q64 SPI Flash OTA** | `app/bsp_spi_flash.c`（待建） | Phase 4 任务 |
| **电机传感器实现** | `driver/mod_motor.c` | `Mod_Motor_Get_Speed()` 和 `Mod_Motor_Angle()` 当前返回 0.0f（占位） |
| **CAN 滤波器细化** | `app/bsp_can.c` | 当前全通（掩码 0），需要按源地址过滤 |

## Flash 分区（F429 片上 2MB，用 1MB）

```
0x08000000 - 0x0800FFFF   Bootloader       64 KB   Sector 0-3
0x08010000 - 0x0801FFFF   OTA 参数区       64 KB   Sector 4 (append-only 日志)
0x08020000 - 0x0807FFFF   App A (活跃槽)   384 KB  Sector 5-7
0x08080000 - 0x080DFFFF   App B (备用槽)   384 KB  Sector 8-10
0x080E0000 - 0x080FFFFF   (预留)           128 KB  Sector 11
```

## RAM 区划（修复后）

```
0x20000000 - 0x2001FFFF   主 SRAM 128KB    DMA 缓冲/全局/栈
0x10000000 - 0x1000FFFF   CCM 64KB        FreeRTOS 堆 (heap_4 ucHeap)
```

## 关键技术约束

### CCM 与 DMA
- FreeRTOS 堆位于 CCM（CPU 独占总线），**DMA 控制器无法访问 CCM**
- 任何 DMA 缓冲必须静态分配（`.bss` 或全局数组，落在主 SRAM `0x2000xxxx`）
- 禁止对 `pvPortMalloc` 返回的指针进行 DMA 操作
- 参考：`project/display_ecu_f429/ccm_dma_guideline.md`

### 真 AB 分区机制
- **不是** A 运行、B 当备份仓库的老方案
- 每个槽独立链接一份镜像（通过各自的 `.sct` scatter 文件），槽 A 链接到 `0x08020000`，槽 B 链接到 `0x08080000`
- Bootloader 根据 `active_partition` 设 `SCB->VTOR` 后跳转
- 升级：YMODEM 写**非活跃槽** → 校验 reset-handler 地址（防错包）→ 翻转 active → 重启
- 回滚：CRC 失败切活跃槽标记，零 Flash 搬运
- `boot_decision.c` 是核心：负责 CRC 校验 + 选址 + 回滚决策
- `task_query.c` 用 `SCB->VTOR` 自证运行槽，不读 OTA 参数区

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

## 工程结构要点

```
display_ecu_f429/
  app/                  BSP 驱动层（bsp_can/led/key/log/uart）
  bootloader/           Bootloader（YMODEM + OTA 决策）
  firmware/             CMSIS/Startup/system_stm32f4xx
  task/                 FreeRTOS 任务（CAN 收发、协议、心跳、按键、查询）
  third_lib/            FreeRTOS v11.3.0
  mdk/                  Keil 工程 + scatter 文件
    app.uvprojx         → 双 Target: stm32f429 (A) / stm32f429_b (B)
    boot.uvprojx        → Bootloader
    app.sct / app_b.sct → 各槽 scatter
  protocol/             CAN ID 解析 Python 工具
  tools/                YMODEM 发送工具
  *.md                  变更记录和设计文档
```

## 编译要点

- **编译器**：armcc V5.06（ARM Compiler 5）
- **C 标准**：C99
- app 和 boot 是**独立工程**，手动切换编译
- app 工程有两个 Target：`stm32f429`（A 槽）和 `stm32f429_b`（B 槽）
- 编译 app B 时如果要烧入 A 槽地址（调试），需切换 .sct 起始地址
- Scatter 文件通过 `UmfTarg=0` 强制使用，不会被子目标对话框覆盖

## 编码规范

- BSP 层：`app/bsp_<module>.c/h` — 硬件抽象
- 任务层：`task/mod_<module>.c/h` 或 `task/task_<module>.c/h` — 业务逻辑
- 日志：`LOG_E`/`LOG_W`/`LOG_I`/`LOG_D`（`bsp_log.h`）
- CAN 协议 ID 位域集中定义在 `task/mod_comm_can.h`
- 注释和文档用中文

## 下一步工作建议

1. **LTDC LCD 驱动** — 点亮屏幕是后续所有 UI 工作的前提
2. **动力域 ECU** — 将 `power_ecu_f103` 的三个 task 文件和两个 driver 文件从空壳实现为完整业务逻辑（CAN 收发、编码器测速、PWM 电机控制、PID 闭环）
3. **LVGL 仪表盘 UI** — 需要先完成 LCD 驱动
4. **电机传感器** — 将 `mod_motor.c` 的占位函数对接实际编码器

## 参考文档

- 完整方案：`docs/Car_Panel_F429_Project_Plan.md`
- 真 AB 分区设计：`project/display_ecu_f429/ab_partition_design.md`
- CCM/DMA 规范：`project/display_ecu_f429/ccm_dma_guideline.md`
- 变更记录：`project/display_ecu_f429/change_*.md`（共 3 份，按步骤 1→2→3 递进）
