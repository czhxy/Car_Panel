# Car_Panel - 汽车双 ECU 仪表盘项目

> 双 MCU（STM32F407 + STM32F103）+ CAN 总线架构，模拟汽车"动力域 ECU + 显示域 ECU"

## 项目概述

- **动力域 ECU**：STM32F103C8T6 — 编码器测速、PWM 电机控制、PID 闭环、故障检测
- **显示域 ECU**：STM32F407ZGT6 — Bootloader/IAP + LVGL 仪表盘 + CAN 通信
- **通信总线**：500 kbps CAN（终端电阻 120 Ω 两端匹配）
- **OTA 升级**：串口/YMODEM → W25Q64 → Bootloader 搬运

## 仓库结构

```
Car_Panel/
├── Car_Panel_Project_Plan.md   # 项目方案文档（主文档）
├── display_ecu_f407/           # 显示域工程（Bootloader + App）
├── power_ecu_f103/             # 动力域工程
├── docs/                       # 项目文档与原理图
└── tools/                      # 工具脚本
```

## 开发阶段

| Phase | 内容 | 状态 |
|-------|------|------|
| 0 | 硬件采购与最小系统验证 | 待开始 |
| 1 | Bootloader + 最小 App | 待开始 |
| 2 | 动力域最小闭环 | 待开始 |
| 3 | CAN 通信闭环 | 待开始 |
| 4 | LVGL 仪表盘 | 待开始 |
| 5 | OTA 完整流程 | 待开始 |
| 6 | 闭环控制 + 诊断 | 待开始 |
| 7 | ESP32 WiFi 网关（可选） | 待开始 |

## 开发环境

- IDE: Keil MDK-ARM v5
- MCU 配置: STM32CubeMX
- 调试器: ST-Link V2 / J-Link
- RTOS: FreeRTOS（F407 App）
- GUI: LVGL v8

## 参考文档

详细方案见 [Car_Panel_Project_Plan.md](Car_Panel_Project_Plan.md)
