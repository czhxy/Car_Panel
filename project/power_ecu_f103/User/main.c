#include "main.h"

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	Sysclock_Init();
	Task_Comm_Can_Init();
	Task_Uart_Init();
	Task_Motor_Ctl_Init();

	/* 控制台就绪提示 */
	Usart_SendString("\r\n=== UART Console Ready ===\r\n> ");

	while (1)
	{
		if(tpf.task_period_5ms)
		{
			tpf.task_period_5ms = 0;
			Task_Motor_Ctl();
		}
		if(tpf.task_period_10ms)
		{
			tpf.task_period_10ms = 0;
			Task_Comm_Rx_Can();
			Task_Comm_Tx_Can();
			Task_Can_Motor_Updata();
		}
		if(tpf.task_period_20ms)
		{
			tpf.task_period_20ms = 0;
			Task_Uart_Tx();
			Task_Uart_Rx();
		}
		if(tpf.task_period_500ms)
		{
			tpf.task_period_500ms = 0;
			Task_Can_Heartbeat_Updata();
		}
	}
}
