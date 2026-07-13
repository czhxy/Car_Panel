#include "main.h"

int main(void)
{	
	Sysclock_Init();
	Task_Comm_Can_Init();
	Task_Uart_Init();
	Task_Motor_Ctl_Init();
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
		}
		if(tpf.task_period_20ms)
		{	
			tpf.task_period_20ms = 0;
			Task_Uart_Tx();
			Task_Uart_Rx();
		}
	}
}
