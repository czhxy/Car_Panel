/**
  ******************************************************************************
  * @file    mod_comm_uart.h
  * @brief   UART 通信框架 — 参考 mod_comm_can 的「队列 + 机制 + 弱符号回调」模式
  *
  * 架构（与 CAN 对齐）：
  *   RX: USART1_IRQHandler → Mod_Uart_RxIRQHandler() → UartRxQueue(字节队列)
  *       → Task_UartRx 取字节 → Mod_Uart_RxByte 拼包(0xAA 0x55 type len data crc16)
  *       → 弱符号回调 ModCommUart_OnRxPacket(type, data, len)
  *   TX: Mod_Uart_SendPacket() 组包入 UartTxQueue
  *       → Task_UartTx → Mod_Uart_TxSend 统一消费 → UART_SendArray() 发送
  *
  * 分层解耦：
  *   - driver/usart.c  : 硬件原语（UART_Init / UART_SendArray / fputc），不依赖本模块
  *   - stm32f4xx_it.c  : USART1_IRQHandler 仅转发到本模块（与 CAN1_RX0_IRQHandler 对齐）
  *   - 本模块 (mod 层) : 队列管理 + 协议编解码 + 发送原语（任务放 task/task_comm_uart）
  *   - 业务层           : 强符号覆盖 ModCommUart_OnRxPacket，不直接碰串口
  *
  * 帧格式: [0xAA][0x55][type][len][data...][crc16_hi][crc16_lo]
  *   CRC16 poly 0x1021, 初始值 0x0000, MSB first（与 YMODEM / PC 工具一致）
  *   注意: len==0 的帧 PC 端查询指令 [AA 55 type 00] 不带 CRC，本框架对 len==0 直接回调
  ******************************************************************************
  */
#ifndef __MOD_COMM_UART_H
#define __MOD_COMM_UART_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* ---- 队列深度 ---- */
#define UART_RX_QUEUE_LENGTH   256U   /* 字节队列（ISR → RX 任务） */
#define UART_TX_QUEUE_LENGTH   8U     /* 发送包队列（业务 → TX 任务） */

/* ---- 日志队列 ---- */
#define UART_LOG_QUEUE_LENGTH   16U   /* 日志字符串队列深度 */
#define UART_LOG_BUF_SIZE       128U  /* 单条日志最大长度（含 NUL） */

/* ---- 帧尺寸限制 ---- */
#define UART_PKT_MAX_DATA      16U    /* 单帧最大负载长度（chip info 应答 13B） */
#define UART_PKT_MAX_LEN       (4U + UART_PKT_MAX_DATA + 2U)  /* 头4 + data + crc2 */

/* ---- 帧头常量 ---- */
#define UART_PKT_HEADER1       0xAA
#define UART_PKT_HEADER2       0x55

/* ---- API 声明 ---- */
void Mod_Uart_Init(void);                        /* 创建 RX/TX/日志队列（任务启动前调用） */
void Mod_Uart_RxIRQHandler(void);                /* USART1_IRQHandler 中调用 */
bool Mod_Uart_SendPacket(uint8_t type, const uint8_t *data, uint8_t len); /* 组包并入 TX 队列 */
bool Mod_Uart_TxSend(TickType_t timeout);        /* 发送任务：从 TX 队列取一个包并发送 */
bool Mod_Uart_LogSend(TickType_t timeout);       /* 发送任务：从日志队列取一条日志并发送 */

/* 日志队列满丢弃计数（LOG 尽力而为，队列满丢新日志） */
extern volatile uint32_t uart_log_drop_cnt;
bool Mod_Uart_RxDequeue(uint8_t *ch, TickType_t timeout);  /* 接收任务：从字节队列取一字节 */
void Mod_Uart_RxByte(uint8_t ch);                /* 接收任务：喂一个字节给拼包状态机 */

/* 弱符号回调：应用层定义同名强符号覆盖 */
void ModCommUart_OnRxPacket(uint8_t type, const uint8_t *data, uint8_t len);

#endif /* __MOD_COMM_UART_H */
