#ifndef __USART6_H
#define __USART6_H
#include "stm32f4xx.h"
#include <stddef.h>

/* USART6 (PC6=TX / PC7=RX) 临时调试串口，专供 VOFA+ firewater 波形输出。
 * 测试完成后可整段移除（含 task/task_vofa.c、mdk 工程引用）。 */

void UART6_Init(void);
void UART6_SendByte(uint8_t Byte);
void UART6_SendString(const char *String);

#endif /* __USART6_H */
