#include "task_motor_ctl.h"

void Task_Motor_Ctl_Init(void)
{
	Mod_Motor_Init();
}

void Task_Motor_Ctl(void)
{
	/* 每 5ms 刷新左右编码器实测值，写入 motor_left/motor_right 全局结构体 */
	Mod_Motor_Update();
}

/** 组装并发送单电机状态帧 0x110 */
static void motor_send_status(Motor_Struct *m, uint8_t func_field)
{
	CanTxMsg tx;
	CanStatusMotor st;

	memset(&tx, 0, sizeof(tx));
	memset(&st, 0, sizeof(st));

	st.motor_speed   = m->cur_speed_enc;
	st.motor_current = m->cur_current;
	st.encoder_angle = m->cur_angle_enc;
	st.status        = m->status;
	st.temperature   = m->temperature;

	tx.ExtId = CAN_ID_BUILD(CAN_PRIO_REALTIME, CAN_SELF_ADDR, CAN_ADDR_MAINBOARD,
	                        CAN_FTYPE_NORMAL, MODE_ID_STATUS_MOTOR, func_field);
	tx.IDE = CAN_Id_Extended;
	tx.RTR = CAN_RTR_Data;
	tx.DLC = 8;
	memcpy(tx.Data, &st, sizeof(st));

	Can_Tx_Event(tx);
}

void Task_Can_Motor_Updata(void)
{
	/* ===== TX 电机状态帧 0x110（10ms）=====
	 * func_field 区分电机：0x00=左, 0x01=右 */
	motor_send_status(&motor_left,  0x00U);
	motor_send_status(&motor_right, 0x01U);
}
