# Car_Panel - CLAUDE.md

## 项目简介
汽车双 ECU 仪表盘项目：STM32F407（显示域）+ STM32F103（动力域）+ CAN + LVGL + OTA

## 关键文件
- `docs/Car_Panel_Project_Plan.md` — 完整项目方案（v2.1），包含引脚规划、Flash 分区、CAN 协议、OTA 路线等
- `docs/schematic/STM32F407Z开发板--原理图-2019M(新液晶接口).PDF` — F407 开发板原理图
- `docs/notes/Conversation_Log.md` — 项目对话记录
- `docs/notes/汽车双ECU仪表盘项目方案.md` — 项目方案 v1（历史归档）

## 架构要点
- **F407 显示域**：Bootloader(128KB) + App(768KB) + 参数区(128KB)，运行 FreeRTOS + LVGL
- **F103 动力域**：单 App(64KB)，裸机 + 状态机
- **CAN 波特率**：500 kbps，F407 节点 0x0A，F103 节点 0x0B
- **OTA**：YMODEM → W25Q64 → Bootloader 搬运 → App

## 编码规范
- BSP 层：`bsp_<module>.c/h`，硬件抽象
- 应用层：`<module>_app.c/h`，业务逻辑
- 日志：统一使用 `bsp_log.h` 宏（LOG_E/LOG_W/LOG_I/LOG_D）
- CAN 协议：ID 和字段定义在 `common/protocol/can_ids.h`

## 注意事项
- F407 CAN1 必须用 PB8/PB9 remap（PA11/PA12 被 USB 占用）
- F407 无 SDRAM（预留未焊接），LVGL 不能做全屏双缓冲
- App 启动必须设置 SCB->VTOR = 0x08020000
- CAN 过滤器必须配置，否则收不到报文
- LED0/LED1 低电平点亮
- 电机默认停止，只有 CAN 命令 enable=1 才转动
