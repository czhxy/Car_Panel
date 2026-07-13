#include "task_comm_can.h"

void Task_Comm_Can_Init(void)
{
	Mod_Can_Init();
}

void Task_Comm_Rx_Can(void)
{
	/* 取RX队列的数据并进行处理 */
	Can_Rx_Process();
}

void Task_Comm_Tx_Can(void)
{
	uint8_t mb = Can_Tx_Process();
	(void)mb; /* 返回值：成功=mailbox号，NoMailBox=邮箱满稍后重试，空队列时同上 */
}
