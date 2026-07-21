#ifndef __PID_H
#define __PID_H

#include <stdint.h>

/* PID 控制器结构体 */
typedef struct {
	float kp;               /* 比例系数 */
	float ki;               /* 积分系数 */
	float kd;               /* 微分系数 */
	float integral;         /* 积分累加值 */
	float integral_limit;   /* 积分抗饱和上限 */
	float prev_error;       /* 上一次误差 */
	int16_t output_min;     /* 输出下限 */
	int16_t output_max;     /* 输出上限 */
	int16_t output;         /* 当前输出值 */
} PidController;

void Pid_Init(PidController *pid, float kp, float ki, float kd,
              int16_t out_min, int16_t out_max);
int16_t Pid_Compute(PidController *pid, float setpoint, float feedback);
void Pid_Reset(PidController *pid);

#endif /* __PID_H */
