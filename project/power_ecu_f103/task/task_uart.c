#include "task_uart.h"

void Task_Uart_Init(void)
{
	/* 真正初始化 USART1：drv_usart_init() + 注册接收回调（链路验证用 TX 打印） */
	Mod_Usart_Init();
}

void Task_Uart_Rx(void)
{
	/* 获取接收数据，进行处理 */
}

void Task_Uart_Tx(void)
{
	/* 获取发送数据，进行发送 */
}
