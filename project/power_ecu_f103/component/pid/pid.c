#include "pid.h"

/**
 * Pid_Init — 初始化 PID 控制器
 *
 * @param kp      比例系数
 * @param ki      积分系数（已包含 5ms 采样周期，Ki_final = ki × dt）
 * @param kd      微分系数（已包含 5ms 采样周期，Kd_final = kd / dt）
 * @param out_min 输出下限（PWM 限幅负值，如 -999）
 * @param out_max 输出上限（PWM 限幅正值，如 +999）
 */
void Pid_Init(PidController *pid, float kp, float ki, float kd,
              int16_t out_min, int16_t out_max)
{
	pid->kp       = kp;
	pid->ki       = ki;
	pid->kd       = kd;
	pid->integral = 0.0f;
	pid->prev_error  = 0.0f;
	pid->output_min  = out_min;
	pid->output_max  = out_max;

	/* 积分抗饱和上限：输出上限 / Ki，防止积分过大导致退饱和振荡 */
	if (ki > 0.001f)
	{
		pid->integral_limit = (float)(out_max) / ki;
	}
	else
	{
		pid->integral_limit = (float)(out_max);
	}

	pid->output = 0;
}

/**
 * Pid_Compute — 计算 PID 输出（位置式，带积分抗饱和）
 *
 * 公式：output = Kp*err + Ki*∫err + Kd*(err - prev_err)
 *       integral = clamp(integral + err, -limit, +limit)
 *       output   = clamp(Kp*err + Ki*integral + Kd*d_err, out_min, out_max)
 *
 * 微分对误差求导（设定值通常不变，等价于对测量值求导的"derivative on measurement"）
 */
int16_t Pid_Compute(PidController *pid, float setpoint, float feedback)
{
	float error, p_term, i_term, d_term;
	float output_f;

	/* 计算误差 */
	error = setpoint - feedback;

	/* P 项 */
	p_term = pid->kp * error;

	/* I 项：累加积分（dt 缩放，5ms 周期）并限制 */
	pid->integral += error * PID_DT;
	if (pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
	if (pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;
	i_term = pid->ki * pid->integral;

	/* D 项：误差变化率 */
	d_term = pid->kd * (error - pid->prev_error);
	pid->prev_error = error;

	/* 总输出 + 限幅 */
	output_f = p_term + i_term + d_term;
	if (output_f > (float)pid->output_max) output_f = (float)pid->output_max;
	if (output_f < (float)pid->output_min) output_f = (float)pid->output_min;

	pid->output = (int16_t)output_f;
	return pid->output;
}

/**
 * Pid_Reset — 复位 PID 积分和上一次误差（用于启停时清空历史）
 */
void Pid_Reset(PidController *pid)
{
	pid->integral    = 0.0f;
	pid->prev_error  = 0.0f;
	pid->output      = 0;
}
