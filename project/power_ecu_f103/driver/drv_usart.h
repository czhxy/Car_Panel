#ifndef __DRV_USART_H
#define __DRV_USART_H

#include "stm32f10x.h"                  // Device header
#include <stddef.h>
typedef void(*usart_rx_callback)(void);
void usart_rx_cb_register(usart_rx_callback cb);
void drv_usart_init(void);

/* TX 发送（轮询，链路验证打印用） */
void Usart_SendByte(uint8_t b);
void Usart_SendData(const uint8_t *p, uint16_t n);
void Usart_SendString(const char *s);
#endif
