/**
  ******************************************************************************
  * @file    task_query.h
  * @brief   查询协议业务 — 响应 PC 上位机芯片信息查询指令
  *
  * 查询链路已拆分为 UART_TX_Task / UART_RX_Task（mod_comm_uart）：
  * 本模块只提供初始化函数和 ModCommUart_OnRxPacket 强符号覆盖，不直接碰串口。
  *
  * 协议:
  *   PC 发送: [0xAA, 0x55, 0x01, 0x00]
  *   MCU 应答: [0xAA, 0x55, 0x01, len, data(13B), crc16(2B)]
  ******************************************************************************
  */
#ifndef __TASK_QUERY_H
#define __TASK_QUERY_H

void Query_Task_Init(void);

#endif /* __TASK_QUERY_H */
