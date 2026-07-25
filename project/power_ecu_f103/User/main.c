#include "main.h"

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	Sysclock_Init();
	Task_Comm_Can_Init();
	Task_Uart_Init();
	Task_Motor_Ctl_Init();

	/* 检查是否为看门狗复位 */
	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{
		RCC_ClearFlag();
		Mod_Usart_SendString("\r\n[WDT] Reset occurred\r\n> ");
	}
	else
	{
		Mod_Usart_SendString("\r\n=== UART Console Ready ===\r\n> ");
	}

	/* 启动独立看门狗，~1s 超时（务必在所有初始化完成后才启动） */
	Mod_Watchdog_Init();
	
	while (1)
	{
		/* 喂狗：每 5ms 一次，1s 超时下足够安全 */
		if(tpf.task_period_5ms)
		{
			tpf.task_period_5ms = 0;
			Mod_Watchdog_Feed();
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
