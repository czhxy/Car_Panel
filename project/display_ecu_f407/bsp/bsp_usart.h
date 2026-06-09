#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "lwrb/lwrb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* DMA 接收缓冲区大小 */
#define UART_RX_BUF_SIZE    256

/* 串口接收信号量（IDLE 中断 Give，接收任务 Take） */
extern SemaphoreHandle_t xUartRxSem;

/* lwrb 环形缓冲区实例（底层存储直接作为 DMA 目标地址） */
extern lwrb_t s_uart_rx_ring;

/* ---- 函数声明 ---- */
void BSP_USART_Init(void);

/* 发送相关 */
void UART_SendByte(uint8_t Byte);
void UART_SendArray(uint8_t *Array, uint16_t Length);
void UART_SendString(char *String);
void UART_SendNumber(uint32_t Number, uint8_t Length);
int  fputc(int ch, FILE *f);
void SafePrintf(const char *format, ...);

/* ISR 中调用：计算 DMA NDTR 变化量 */
uint16_t UART_CalcRxDelta(uint16_t current_ndtr);

/* ISR 中调用：将接收数据提交到 lwrb 并 Give 信号量 */
void UART_NotifyRxFromISR(BaseType_t *pxHigherPriorityTaskWoken, uint16_t received);

/* 串口接收任务 */
void UARTRxTask(void *pvParameters);

#endif
