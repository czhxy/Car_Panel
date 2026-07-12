#ifndef __DRV_USART_H
#define __DRV_USART_H

#include "stm32f10x.h"                  // Device header
#include <stddef.h>
typedef void(*usart_rx_callback)(void);
void usart_rx_cb_register(usart_rx_callback cb);
void drv_usart_init(void);
#endif
