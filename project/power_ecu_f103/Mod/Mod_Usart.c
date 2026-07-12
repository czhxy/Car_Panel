#include "Mod_Usart.h"

void Mod_Usart_Init(void)
{
	drv_usart_init();
	usart_rx_cb_register(Usart_Rx_Cb);
}
void Usart_Rx_Event(void)
{
	
}

void Usart_Rx_Process(void)
{
	
}

void Usart_Tx_Event(void)
{
	
}

void Usart_Tx_Process(void)
{
	
}
void Usart_Rx_Cb(void)
{
	
}
