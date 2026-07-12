#include "Mod_Comm_Can.h"
#define CAN_TX_QUEUE_BUFFER_SIZE 10
#define CAN_RX_QUEUE_BUFFER_SIZE 10
static QueueType CanTxQueue;
static QueueType CanRxQueue;
static uint8_t CanTxQueueBuffer[CAN_TX_QUEUE_BUFFER_SIZE * sizeof(CanTxMsg)];
static uint8_t CanRxQueueBuffer[CAN_RX_QUEUE_BUFFER_SIZE * sizeof(CanRxMsg)];
void Mod_Can_Init(void)
{
	drv_can_init();
	can_rx_cb_register(Can_Rx_Cb);
	Queue_Init(&CanTxQueue,CanTxQueueBuffer,sizeof(CanTxQueueBuffer),sizeof(CanTxMsg));
	Queue_Init(&CanRxQueue,CanRxQueueBuffer,sizeof(CanRxQueueBuffer),sizeof(CanRxMsg));
}
void Can_Tx_Event(void)
{
	//将数据放到tx队列
}

void Can_Tx_Process(void)
{
	//取出tx队列中的数据并发送
}
	

void Can_Rx_Event(void)
{
	//将数据放进rx队列
	
}

void Can_Rx_Process(void)
{
	//对取出rx队列中的数据进行处理
}
//rx回调函数
void Can_Rx_Cb(CanRxMsg * msg)
{
	
}

