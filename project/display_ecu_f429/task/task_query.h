/**
  ******************************************************************************
  * @file    task_query.h
  * @brief   UART 查询任务 — 响应 PC 上位机芯片信息查询指令
  *
  * 协议:
  *   PC 发送: [0xAA, 0x55, 0x01, 0x00]
  *   MCU 应答: [0xAA, 0x55, 0x01, len, data(13B), crc16(2B)]
  ******************************************************************************
  */
#ifndef __TASK_QUERY_H
#define __TASK_QUERY_H

#include "FreeRTOS.h"
#include "task.h"

void UART_Query_Task(void *pvParameters);

#endif /* __TASK_QUERY_H */
