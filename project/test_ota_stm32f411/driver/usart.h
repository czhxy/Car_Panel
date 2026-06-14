#ifndef __USART_H
#define __USART_H
#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>

void UART_Init(void);
void UART_SendByte(uint8_t Byte);
void UART_SendArray(uint8_t *Array, uint16_t Length);
void UART_SendString(char *String);
void UART_SendNumber(uint32_t Number, uint8_t Length);
int fputc(int ch, FILE *f);
void UART_Printf(char *format, ...);
#endif 
