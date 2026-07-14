#include "main.h"

int main(void)
{
	/* NVIC 分组：4 位抢占优先级 / 0 位子优先级。
	 * 不设的话复位默认 Group0(0 抢占位)，drv_can 里给 RX0/SCE 设的抢占优先级(5/4)会失效，
	 * 两路 CAN 中断变成同优先级、无法按设计抢占。与 drv_usart.c 的设置保持一致。 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
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
			Task_Can_Motor_Updata();/* 10ms 电机状态帧 0x110 */
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
			Task_Can_Heartbeat_Updata();   /* 500ms 心跳帧 0x320 */
		}
	}
}
