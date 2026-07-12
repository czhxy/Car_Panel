#ifndef __MOD_USART_H
#define __MOD_USART_H

#include "stm32f10x.h"                  // Device header
#include "drv_usart.h"

void Mod_Usart_Init(void);
void Usart_Rx_Event(void);
void Usart_Rx_Process(void);
void Usart_Tx_Event(void);
void Usart_Tx_Process(void);
void Usart_Rx_Cb(void);

#endif
