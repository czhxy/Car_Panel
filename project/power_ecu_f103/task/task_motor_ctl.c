#include "task_motor_ctl.h"
#include "task_uart.h"

void Task_Motor_Ctl_Init(void)
{
	Mod_Motor_Init();
}

void Task_Motor_Ctl(void)
{
	/* 每 5ms：刷新编码器实测值 + PID 控制 */
	Mod_Motor_Update();
	Mod_Motor_Process();

	/* VOFA+ FireWater 波形输出（与 PID 计算同频，直接发送绕过 TX 队列） */
	if (vofa_enabled)
		Vofa_SendFrame();
}

/** 组装并发送单电机状态帧 0x110 */
static void Motor_Can_Tx_Event(Motor_Struct *m, uint8_t func_field)
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
	/* 当前只有左电机接好：只发左电机状态帧 0x110, func=0x00。
	 * 原"左右交替发送"会因右电机未接线(转速恒 0)把显示域 rpm 覆盖为 0，
	 * 且显示域暂不区分 func=0x00/0x01。右电机接好后再恢复交替，
	 * 并让显示域按 func 字段区分左右。 */
	Motor_Can_Tx_Event(&motor_left, 0x00U);
}
