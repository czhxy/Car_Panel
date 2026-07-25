#ifndef __TASK_UART_H
#define __TASK_UART_H

#include "stm32f10x.h"                  // Device header
#include "Mod_Usart.h"

/* PID 调试打印开关 */
extern uint8_t pid_debug_enabled;
extern uint8_t pid_debug_side;    /* 0=左电机, 1=右电机 */

/* VOFA+ FireWater 波形输出开关 */
extern uint8_t vofa_enabled;
extern uint8_t vofa_side;         /* 0=左电机, 1=右电机 */
void Vofa_SendFrame(void);

void Task_Uart_Rx(void);
void Task_Uart_Tx(void);
void Task_Uart_Init(void);
#endif
