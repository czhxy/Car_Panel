# Car_Panel - CLAUDE.md

## 项目简介
汽车仪表盘项目显示域：STM32F429IGT6 + FreeRTOS + CAN 通信 + OTA 双分区升级

## 关键文件
- `docs/Car_Panel_Project_Plan.md` — 完整项目方案（v2.1），包含引脚规划、Flash 分区、CAN 协议、OTA 路线等
- `docs/schematic/stm32f429igt6开发板.pdf` — F429 开发板原理图
- `docs/notes/Conversation_Log.md` — 项目对话记录
- `task/mod_comm_can.h` — CAN 协议 ID 定义和 API 声明
- `third_lib/FreeRTOS/inc/FreeRTOSConfig.h` — FreeRTOS 配置（含 ISR 宏映射）

## 架构要点
- **当前阶段**：粗略移植完成，核心任务为 LED 翻转、心跳打印、按键触发 CAN 发送、CAN 接收
- **MCU**：STM32F429IGT6（Cortex-M4F, 180MHz, 1MB Flash, 192KB+64KB SRAM）
- **时钟**：HSE 25MHz → PLL (M=25, N=360, P=2) → SYSCLK=180MHz, HCLK=180MHz, PCLK1=45MHz, PCLK2=90MHz
- **Flash 分区**：Bootloader(64KB) + OTA参数(16KB逻辑/64KB物理) + App A(384KB @ 0x08020000) + App B(512KB)
- **OTA**：双分区内部 Flash 升级，YMODEM-1K 协议
- **FreeRTOS**：v11.3.0, 抢占式调度, 64KB heap_4
- **ISR 路由**：FreeRTOSConfig.h 中通过 `#define vPortSVCHandler SVC_Handler` 等宏将 port.c 的 handler 映射到 CMSIS 向量名（间接路由）

## CAN 配置
- **控制器/引脚**：CAN1, PA11(RX)/PA12(TX), AF_CAN1
- **波特率**：500 kbps (45MHz / 9 / (1+7+2) = 500k)
- **CAN ID**：29 位扩展帧，自节点地址 0x01 (CAN_DEVICE_ID_MAINBOARD)
- **滤波器**：Filter 0, 掩码全 0，接收所有报文 → FIFO0
- **中断**：CAN1_RX0_IRQn, 抢占优先级 5

## 编码规范
- BSP 层：`app/bsp_<module>.c/h`，硬件抽象
- 任务层：`task/mod_<module>.c/h`，业务逻辑
- 日志：统一使用 `bsp_log.h` 宏（LOG_E/LOG_W/LOG_I/LOG_D）
- CAN 协议：ID 和字段定义在 `task/mod_comm_can.h`

## 注意事项
- F429 CAN1 使用 PA11/PA12（与 F407 不同，F429 的 USB OTG FS 可不占用这些引脚，无需 remap）
- F429 跑 180MHz 必须使能 Over-Drive 模式（SystemInit 已处理）
- App 启动必须设置 SCB->VTOR = 0x08020000
- **不要手动调用 SysTick_Config()** — FreeRTOS 的 `vPortSetupTimerInterrupt()` 自动配置
- **不要定义空的 SVC_Handler/PendSV_Handler** — FreeRTOS 通过宏映射接管，空闲实现会导致调度器失效
- CAN 滤波器必须配置，否则收不到报文
- CAN FIFO1 中断不要使能（滤波器全部分配给 FIFO0，FIFO1 无 ISR）
- CAN 接收中断 ISR 入口 (CAN1_RX0_IRQHandler) 在 `app/main.c` 中实现
- HardFault/MemManage/BusFault/UsageFault 在 `app/main.c` 中有串口诊断输出
- LED (PH10/PH11/PH12, PE3) 低电平点亮
- 按键 (PE2, PI11) 上拉输入，按下为低电平
- CAN 队列必须在 FreeRTOS 任务创建前初始化，避免 RX 任务访问 NULL 指针
- F429 上 APB1 时钟为 45MHz（HCLK/4），不是 F407 的 42MHz，CAN 波特率计算需注意
