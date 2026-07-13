#ifndef __SYSCLOCK_H
#define __SYSCLOCK_H

#include "stm32f10x.h"                  // Device header
typedef struct task_period
{
	uint64_t period_1ms;
	uint64_t period_5ms;
	uint64_t period_10ms;
	uint64_t period_20ms;
	uint64_t period_100ms;
	uint64_t period_200ms;
	uint64_t period_500ms;
	uint64_t period_1000ms;
}TaskPeriod_t;

typedef struct task_period_flag
{
	uint8_t task_period_1ms;
	uint8_t task_period_5ms;
	uint8_t task_period_10ms;
	uint8_t task_period_20ms;
	uint8_t task_period_100ms;
	uint8_t task_period_200ms;
	uint8_t task_period_500ms;
	uint8_t task_period_1000ms;
}TaskPeriodFlag_t;
extern TaskPeriodFlag_t tpf;

void Sysclock_Init(void);
void sys_delay_ms(uint32_t ms);
void sys_delay_us(uint32_t us);
void SysClock_Cb(void);


#endif
