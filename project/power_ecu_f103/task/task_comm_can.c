#include "task_comm_can.h"
void Task_Comm_Can_Init(void)
{
	Mod_Can_Init();
}
void Task_Comm_Rx_Can(void)
{
		//取Rx队列的数据
		
		//对数据进行处理
		Can_Rx_Process();
}

void Task_Comm_Tx_Can(void)
{	
		//取Tx队列的数据
		
		//对数据进行处理
		Can_Tx_Process();
}
