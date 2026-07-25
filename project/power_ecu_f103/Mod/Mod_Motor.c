#include "Mod_Motor.h"
#include "sysclock.h"
#include "Mod_Usart.h"

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

	/* ── CAN 超时保护：200ms 无控制帧则强制刹车 ── */
	{
		uint32_t now = (uint32_t)sysclock_get_ms();
		static uint8_t timeout_printed_l;   /* 防止重复打印 */
		static uint8_t timeout_printed_r;

		/* 左电机 */
		if (motor_left.last_ctrl_ms > 0
		    && (now - motor_left.last_ctrl_ms) > 200U
		    && motor_left.target_speed_enc != 0)
		{
			motor_left.target_speed_enc = 0;
			motor_left.error_code |= MOTOR_ERROR_CAN_TIMEOUT;
			motor_left.status |= MOTOR_STATUS_FAULT;
			if (!timeout_printed_l)
			{
				Uart_Error("CAN L timeout, brake");
				timeout_printed_l = 1;
			}
		}
		else
		{
			timeout_printed_l = 0;  /* CAN 帧恢复，允许下次打印 */
		}

		/* 右电机 */
		if (motor_right.last_ctrl_ms > 0
		    && (now - motor_right.last_ctrl_ms) > 200U
		    && motor_right.target_speed_enc != 0)
		{
			motor_right.target_speed_enc = 0;
			motor_right.error_code |= MOTOR_ERROR_CAN_TIMEOUT;
			motor_right.status |= MOTOR_STATUS_FAULT;
			if (!timeout_printed_r)
			{
				Uart_Error("CAN R timeout, brake");
				timeout_printed_r = 1;
			}
		}
		else
		{
			timeout_printed_r = 0;
		}

		/* ── 编码器异常检测：计数器冻结 + 速度跳变双重检测 ── */
		{
			static int16_t  prev_raw_l, prev_raw_r;
			static uint16_t frozen_cnt_l, frozen_cnt_r;
			static uint8_t  enc_err_printed_l, enc_err_printed_r;
			int16_t raw;

			/* 左电机：计数器冻结检测（目标非零但原始计数器 200ms 不变 → 断线） */
			raw = drv_motor_get_raw_enc(MOTOR_ID_LEFT);
			if (motor_left.target_speed_enc != 0 && raw == prev_raw_l)
			{
				if (++frozen_cnt_l > 40U)
				{
					motor_left.target_speed_enc = 0;
					motor_left.error_code |= MOTOR_ERROR_ENC_LOSS;
					motor_left.status |= MOTOR_STATUS_FAULT;
					if (!enc_err_printed_l)
					{
						Uart_Error("Encoder L lost, brake");
						enc_err_printed_l = 1;
					}
				}
			}
			else { frozen_cnt_l = 0; enc_err_printed_l = 0; }
			prev_raw_l = raw;

			/* 右电机 */
			raw = drv_motor_get_raw_enc(MOTOR_ID_RIGHT);
			if (motor_right.target_speed_enc != 0 && raw == prev_raw_r)
			{
				if (++frozen_cnt_r > 40U)
				{
					motor_right.target_speed_enc = 0;
					motor_right.error_code |= MOTOR_ERROR_ENC_LOSS;
					motor_right.status |= MOTOR_STATUS_FAULT;
					if (!enc_err_printed_r)
					{
						Uart_Error("Encoder R lost, brake");
						enc_err_printed_r = 1;
					}
				}
			}
			else { frozen_cnt_r = 0; enc_err_printed_r = 0; }
			prev_raw_r = raw;

			/* 速度跳变检测：相邻 5ms 变化 >50rpm → 噪声/干扰 */
			{
				static int16_t prev_speed_l, prev_speed_r;
				int16_t jump;

				jump = motor_left.cur_speed_enc - prev_speed_l;
				prev_speed_l = motor_left.cur_speed_enc;
				if ((jump > 1500 || jump < -1500) && motor_left.target_speed_enc != 0
				    && !enc_err_printed_l)
				{
					motor_left.target_speed_enc = 0;
					motor_left.error_code |= MOTOR_ERROR_ENC_LOSS;
					motor_left.status |= MOTOR_STATUS_FAULT;
					Uart_Error("Encoder L glitch, brake");
					enc_err_printed_l = 1;
				}

				jump = motor_right.cur_speed_enc - prev_speed_r;
				prev_speed_r = motor_right.cur_speed_enc;
				if ((jump > 1500 || jump < -1500) && motor_right.target_speed_enc != 0
				    && !enc_err_printed_r)
				{
					motor_right.target_speed_enc = 0;
					motor_right.error_code |= MOTOR_ERROR_ENC_LOSS;
					motor_right.status |= MOTOR_STATUS_FAULT;
					Uart_Error("Encoder R glitch, brake");
					enc_err_printed_r = 1;
				}
			}
		}

		/* ── 堵转检测：目标≥2rpm、速度≈0、持续 200ms ── */
		{
			static uint16_t stall_cnt_l, stall_cnt_r;
			static uint8_t  stall_printed_l, stall_printed_r;

			if ((motor_left.target_speed_enc > 20 || motor_left.target_speed_enc < -20)
			    && (motor_left.cur_speed_enc < 10 && motor_left.cur_speed_enc > -10))
			{
				if (++stall_cnt_l > 40U)
				{
					motor_left.target_speed_enc = 0;
					motor_left.error_code |= MOTOR_ERROR_STALL;
					motor_left.status |= MOTOR_STATUS_FAULT;
					if (!stall_printed_l)
					{
						Uart_Error("Motor L stall, brake");
						stall_printed_l = 1;
					}
				}
			}
			else
			{
				stall_cnt_l = 0;
				stall_printed_l = 0;
			}

			if ((motor_right.target_speed_enc > 20 || motor_right.target_speed_enc < -20)
			    && (motor_right.cur_speed_enc < 10 && motor_right.cur_speed_enc > -10))
			{
				if (++stall_cnt_r > 40U)
				{
					motor_right.target_speed_enc = 0;
					motor_right.error_code |= MOTOR_ERROR_STALL;
					motor_right.status |= MOTOR_STATUS_FAULT;
					if (!stall_printed_r)
					{
						Uart_Error("Motor R stall, brake");
						stall_printed_r = 1;
					}
				}
			}
			else
			{
				stall_cnt_r = 0;
				stall_printed_r = 0;
			}
		}
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
