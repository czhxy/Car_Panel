#include "Mod_Comm_Can.h"

static QueueType CanTxQueue;
static QueueType CanRxQueue;
static CanTxMsg CanTxQueueBufferPool[20];
static CanRxMsg CanRxQueueBufferPool[20];

typedef struct {
    uint8_t motor_tx_err_count;
    uint8_t motor_rx_err_count;
} ModCanMotor_EventErrCount;
ModCanMotor_EventErrCount event_err_count = {0};

void Mod_Can_Init(void)
{
	drv_can_init();
	can_rx_cb_register(Can_Rx_Cb);
	Queue_Init(&CanTxQueue,CanTxQueueBufferPool,sizeof(CanTxQueueBufferPool),sizeof(CanTxQueueBufferPool[0]));
	Queue_Init(&CanRxQueue,CanRxQueueBufferPool,sizeof(CanRxQueueBufferPool),sizeof(CanRxQueueBufferPool[0]));
}
bool Can_Tx_Event(CanTxMsg TxMsg)
{
	if (Queue_Put(&CanTxQueue, &TxMsg))
		return true;
	event_err_count.motor_tx_err_count++;
	return false;
}

CanTxMsg TxPack;
CanRxMsg RxPack;
uint8_t Can_Tx_Process(void)
{
	uint8_t mb;
	uint8_t send_cnt = 0;        /* 本周期已发送帧数 */

	/* 循环出队直到队列空或邮箱满（CAN_NO_MB），
	 * F103 有 3 个 TX 邮箱，每帧 ~130µs，10ms 内可排空大量积压 */
	while (send_cnt < 8 && Queue_Query(&CanTxQueue, &TxPack))
	{
		mb = CAN_Transmit(CAN1, &TxPack);
		if (mb == CAN_NO_MB)
		{
			break;               /* 3 个邮箱都忙，下周期再试 */
		}
		Queue_Get(&CanTxQueue, &TxPack);
		send_cnt++;
	}
	return send_cnt;
}

bool Can_Rx_Event(CanRxMsg RxMsg)
{
	if (Queue_Put(&CanRxQueue, &RxMsg))
		return true;
	event_err_count.motor_rx_err_count++;
	return false;
}

__attribute__((weak)) void TaskCanMotor_RxCallback(CanRxMsg motor_pack) {}
void Can_Rx_Process(void)
{
	/* 读取RX队列中的数据进行处理 */
	if(Queue_Query(&CanRxQueue,&RxPack))
	{
		//将can内容解析出来，准备传给电机
		TaskCanMotor_RxCallback(RxPack);
		Queue_Get(&CanRxQueue,&RxPack);
	}
}

/* CAN RX 回调函数 */
void Can_Rx_Cb(CanRxMsg msg)
{
	Can_Rx_Event(msg);
}
