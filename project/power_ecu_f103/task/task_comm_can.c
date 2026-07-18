#include "task_comm_can.h"
#include "CAN_Protocol.h"
#include "Mod_Motor.h"
#include "drv_usart.h"
#include "sysclock.h"
#include <string.h>

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
void TaskCanMotor_RxCallback(CanRxMsg motor_pack)
{
	CanProtocolId id;

	if (motor_pack.IDE != CAN_Id_Extended) return;   /* 协议统一用 29 位扩展帧 */
	CanProto_DecodeId(motor_pack.ExtId, &id);

	if (id.frame_type != CAN_FTYPE_NORMAL) return;
	if (id.dst_addr != CAN_SELF_ADDR && id.dst_addr != CAN_ADDR_BROADCAST) return;

	switch (id.mode_id)
	{
		case MODE_ID_CTRL_LF:   /* 0x020 左前轮转向+轮毂控制（显示域→动力域） */
			if (motor_pack.DLC >= 4)
			{
				/* 显示域实际线序(小端): data[0..1]=speed(rpm×10), data[2..3]=angle(°×10) */
				motor_left.target_speed_enc = (int16_t)((uint16_t)motor_pack.Data[0]
				                                      | ((uint16_t)motor_pack.Data[1] << 8));
				motor_left.target_angle_enc = (int16_t)((uint16_t)motor_pack.Data[2]
				                                      | ((uint16_t)motor_pack.Data[3] << 8));
				motor_left.rx_ctrl_count++;
				motor_left.last_ctrl_ms = (uint32_t)sysclock_get_ms();
			}
			break;
		case MODE_ID_CTRL_RF:   /* 0x021 右前轮转向+轮毂控制（显示域→动力域） */
			if (motor_pack.DLC >= 4)
			{
				motor_right.target_speed_enc = (int16_t)((uint16_t)motor_pack.Data[0]
				                                       | ((uint16_t)motor_pack.Data[1] << 8));
				motor_right.target_angle_enc = (int16_t)((uint16_t)motor_pack.Data[2]
				                                       | ((uint16_t)motor_pack.Data[3] << 8));
				motor_right.rx_ctrl_count++;
				motor_right.last_ctrl_ms = (uint32_t)sysclock_get_ms();
			}
			break;
		case MODE_ID_QUERY_FAST:   /* 0x080 链路测试：回显显示域串口透传过来的数据（测试脚手架） */
			Usart_SendString("RX 0x080: ");
			Usart_SendData(motor_pack.Data, motor_pack.DLC);
			Usart_SendString("\r\n");
			break;
		/* MODE_ID_ESTOP(0x000)/BRAKE(0x001) 等以后按需扩展 */
		default:
			break;
	}
}



/* ===== TX：心跳帧 0x320（500ms）=====
 * 用 MODE_ID_HEARTBEAT(0x320)，不用 CAN_HEARTBEAT_ID 宏（它嵌的是 0x000）。
 * 广播本机状态/运行时长/故障码供显示域监测存活。 */
void Task_Can_Heartbeat_Updata(void)
{
	CanTxMsg tx;
	CanHeartbeatData hb;

	memset(&tx, 0, sizeof(tx));
	memset(&hb, 0, sizeof(hb));

	hb.status     = motor_left.status | motor_right.status;
	hb.uptime     = (uint16_t)(sysclock_get_ms() / 1000U);
	hb.error_code = motor_left.error_code | motor_right.error_code;

	tx.ExtId = CAN_ID_BUILD(CAN_PRIO_HEARTBEAT, CAN_SELF_ADDR, CAN_ADDR_BROADCAST,
	                        CAN_FTYPE_NORMAL, MODE_ID_HEARTBEAT, 0x00U);
	tx.IDE = CAN_Id_Extended;
	tx.RTR = CAN_RTR_Data;
	tx.DLC = 8;
	memcpy(tx.Data, &hb, sizeof(hb));

	Can_Tx_Event(tx);
}
