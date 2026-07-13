#include "Mod_Comm_Can.h"

static QueueType CanTxQueue;
static QueueType CanRxQueue;
static CanTxMsg CanTxQueueBufferPool[20];
static CanRxMsg CanRxQueueBufferPool[20];

void Mod_Can_Init(void)
{
	drv_can_init();
	can_rx_cb_register(Can_Rx_Cb);
	Queue_Init(&CanTxQueue,CanTxQueueBufferPool,sizeof(CanTxQueueBufferPool),sizeof(CanTxQueueBufferPool[0]));
	Queue_Init(&CanRxQueue,CanRxQueueBufferPool,sizeof(CanRxQueueBufferPool),sizeof(CanRxQueueBufferPool[0]));
}
void Can_Tx_Event(CanTxMsg * TxMsg)
{
	if(TxMsg == NULL) return;
	if(!Queue_Put(&CanTxQueue,TxMsg))
	{
		/* TX 队列满：数据丢失，应有告警记录 */
	}
}

CanTxMsg TxPack;
CanRxMsg RxPack;
uint8_t  Can_Tx_Process(void)
{
	uint8_t mb;
	if(Queue_Query(&CanTxQueue,&TxPack))
	{
		mb = CAN_Transmit(CAN1,&TxPack);
		if(mb != CAN_NO_MB)
		{
			Queue_Get(&CanTxQueue,&TxPack);
		}
		return mb;
	}
	return CAN_TxStatus_NoMailBox;
}

void Can_Rx_Event(void)
{
	/* 将数据放进RX队列 */
	Queue_Put(&CanRxQueue,&RxPack);
}

void Can_Rx_Process(void)
{
	/* 读取RX队列中的数据进行处理 */
	if(Queue_Query(&CanRxQueue,&RxPack))
	{	
		//将can内容解析出来，准备传给电机
		Queue_Get(&CanRxQueue,&RxPack);
	}	
}

/* RX 回调函数 */
void Can_Rx_Cb(CanRxMsg * msg)
{
	Can_Rx_Event();
}
