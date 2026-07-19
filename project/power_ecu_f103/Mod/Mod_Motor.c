#include "Mod_Motor.h"

/* 全局电机状态实例：零初始化，目标值由 CAN RX 写入，实测值由 drv_motor 填充 */
Motor_Struct motor_left  = {0};
Motor_Struct motor_right = {0};

/* PID 控制器：左右电机各独立 PID 实例 */
static PidController pid_left;
static PidController pid_right;
static uint8_t pid_inited;

void Mod_Motor_Init(void)
{
	drv_motor_init(MOTOR_ID_LEFT);
	drv_motor_init(MOTOR_ID_RIGHT);
}

/** Mod_Motor_Update — 由任务层 5ms 周期调用，读取左右编码器并更新实测值 */
void Mod_Motor_Update(void)
{
	drv_motor_update(MOTOR_ID_LEFT);
	drv_motor_update(MOTOR_ID_RIGHT);
}

/**
 * Mod_Motor_Process — 5ms 周期 PID 转速闭环控制
 *
 * 目标转速从 CAN 控制帧（MODE_ID_CTRL_LF/CTRL_RF）写入 target_speed_enc，
 * 实测转速由 drv_motor_update → cur_speed_enc。
 *
 * 停机逻辑：target_speed_enc == 0 → 完全关闭电机（PWM=0, EN=0）+ 复位 PID，
 *           防止零速微振并节省功耗。
 */
void Mod_Motor_Process(void)
{
	/* 首次调用时初始化 PID */
	if (!pid_inited)
	{
		Pid_Init(&pid_left,  2.0f, 0.1f, 0.5f, -PWM_MAX, PWM_MAX);
		Pid_Init(&pid_right, 2.0f, 0.1f, 0.5f, -PWM_MAX, PWM_MAX);
		pid_inited = 1;
	}

	/* ── 左电机 ── */
	if (motor_left.target_speed_enc == 0)
	{
		drv_motor_set_enable(MOTOR_ID_LEFT, 0);
		Pid_Reset(&pid_left);
	}
	else
	{
		int16_t pwm;
		drv_motor_set_enable(MOTOR_ID_LEFT, 1);
		pwm = Pid_Compute(&pid_left,
		                  (float)motor_left.target_speed_enc,
		                  (float)motor_left.cur_speed_enc);
		drv_motor_set_pwm(MOTOR_ID_LEFT, pwm);
	}

	/* ── 右电机 ── */
	if (motor_right.target_speed_enc == 0)
	{
		drv_motor_set_enable(MOTOR_ID_RIGHT, 0);
		Pid_Reset(&pid_right);
	}
	else
	{
		int16_t pwm;
		drv_motor_set_enable(MOTOR_ID_RIGHT, 1);
		pwm = Pid_Compute(&pid_right,
		                  (float)motor_right.target_speed_enc,
		                  (float)motor_right.cur_speed_enc);
		drv_motor_set_pwm(MOTOR_ID_RIGHT, pwm);
	}
}
