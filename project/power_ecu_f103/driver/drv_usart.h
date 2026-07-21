#ifndef __DRV_USART_H
#define __DRV_USART_H

#include "stm32f10x.h"                  // Device header
#include <stddef.h>

#define UART_RX_FRAME_SIZE 32

/* RX 回调：ISR 将收到的数据推送给模块层 */
typedef void(*usart_rx_callback)(const uint8_t *buf, uint16_t len);
void usart_rx_cb_register(usart_rx_callback cb);
void drv_usart_init(void);

/* USART 硬件已就绪后，使能 RX 中断（须在回调注册之后调用） */
void drv_usart_start_rx(void);

/* TX 发送（轮询，链路验证打印用） */
void Usart_SendByte(uint8_t b);
void Usart_SendData(const uint8_t *p, uint16_t n);
void Usart_SendString(const char *s);
#endif
