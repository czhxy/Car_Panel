#include "Mod_Motor.h"

/* 全局电机状态实例：零初始化，目标值由 CAN RX 写入，实测值由 drv_motor 填充 */
Motor_Struct motor_left  = {0};
Motor_Struct motor_right = {0};

/* PID 控制器：左右电机各独立 PID 实例 */
PidController pid_left;
PidController pid_right;
static uint8_t  pid_inited;
static int16_t  prev_target_l;   /* 左电机上次目标值，用于检测方向变化 */
static int16_t  prev_target_r;   /* 右电机上次目标值 */

void Mod_Motor_Init(void)
{
	drv_motor_init(MOTOR_ID_LEFT);
	drv_motor_init(MOTOR_ID_RIGHT);
}

/** Mod_Motor_Update — 由任务层 5ms 周期调用，读取编码器并更新实测值 */
void Mod_Motor_Update(void)
{
	drv_motor_update(MOTOR_ID_LEFT);
	drv_motor_update(MOTOR_ID_RIGHT);
}

/** 宏：检测目标符号是否改变（异号且任一非零），用于方向反转时复位积分 */
#define TARGET_SIGN_CHANGED(cur, prev) \
	(((cur) > 0 && (prev) < 0) || ((cur) < 0 && (prev) > 0))

/**
 * Mod_Motor_Process — 5ms 周期 PID 转速闭环控制
 *
 * 停机逻辑：target_speed_enc == 0 → 完全关闭电机（PWM=0）+ 复位 PID
 * 方向反转：目标符号改变 → 复位积分，避免旧方向积分延缓反转
 */
void Mod_Motor_Process(void)
{
	int16_t pwm;

	/* 首次调用时初始化 PID */
	if (!pid_inited)
	{
		Pid_Init(&pid_left,  1.0f, 0.1f, 0.3f, -PWM_MAX, PWM_MAX);
		Pid_Init(&pid_right, 1.0f, 0.1f, 0.3f, -PWM_MAX, PWM_MAX);
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
		if (TARGET_SIGN_CHANGED(motor_left.target_speed_enc, prev_target_l))
		{
			Pid_Reset(&pid_left);
		}
		drv_motor_set_enable(MOTOR_ID_LEFT, 1);
		pwm = Pid_Compute(&pid_left,
		                  (float)motor_left.target_speed_enc,
		                  (float)motor_left.cur_speed_enc);
		drv_motor_set_pwm(MOTOR_ID_LEFT, pwm);
	}
	prev_target_l = motor_left.target_speed_enc;

	/* ── 右电机 ── */
	if (motor_right.target_speed_enc == 0)
	{
		drv_motor_set_enable(MOTOR_ID_RIGHT, 0);
		Pid_Reset(&pid_right);
	}
	else
	{
		if (TARGET_SIGN_CHANGED(motor_right.target_speed_enc, prev_target_r))
		{
			Pid_Reset(&pid_right);
		}
		drv_motor_set_enable(MOTOR_ID_RIGHT, 1);
		pwm = Pid_Compute(&pid_right,
		                  (float)motor_right.target_speed_enc,
		                  (float)motor_right.cur_speed_enc);
		drv_motor_set_pwm(MOTOR_ID_RIGHT, pwm);
	}
	prev_target_r = motor_right.target_speed_enc;
}
