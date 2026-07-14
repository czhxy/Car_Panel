#include "task_motor_ctl.h"
void Task_Motor_Ctl_Init(void)
{
	Mod_Motor_Init();
}
void Task_Motor_Ctl(void)
{	
	//根据can接收并还原出来的数据进行电机控制
}
void Task_Can_Motor_Updata(void)
{
	//将电机数据推送到电机队列中
	
	/* ===== TX：电机状态帧 0x110（20ms）=====
 * 组装实测转速/电流/角度/状态/温度入 TX 队列，由 10ms 的 Can_Tx_Process 发出。
 * 实测值在 drv_motor 实现前为 0。 */
	CanTxMsg tx;
	CanStatusMotor st;

	memset(&tx, 0, sizeof(tx));
	memset(&st, 0, sizeof(st));

	st.motor_speed   = motor.cur_speed_enc;
	st.motor_current = motor.cur_current;
	st.encoder_angle = motor.cur_angle_enc;
	st.status        = motor.status;
	st.temperature   = motor.temperature;

	tx.ExtId = CAN_ID_BUILD(CAN_PRIO_REALTIME, CAN_SELF_ADDR, CAN_ADDR_MAINBOARD,
	                        CAN_FTYPE_NORMAL, MODE_ID_STATUS_MOTOR, 0x00U);
	tx.IDE = CAN_Id_Extended;
	tx.RTR = CAN_RTR_Data;
	tx.DLC = 8;
	memcpy(tx.Data, &st, sizeof(st));

	Can_Tx_Event(tx);
}
