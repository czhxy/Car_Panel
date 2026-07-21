#ifndef __TASK_UART_H
#define __TASK_UART_H

#include "stm32f10x.h"                  // Device header
#include "Mod_Usart.h"

/* 编码器调试打印开关 */
extern uint8_t enc_debug_enabled;

void Task_Uart_Rx(void);
void Task_Uart_Tx(void);
void Task_Uart_Init(void);
#endif
