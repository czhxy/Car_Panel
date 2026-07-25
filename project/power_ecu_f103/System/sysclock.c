#include "sysclock.h"
#include <stdint.h>
#include <stddef.h>
static volatile uint64_t sys_tick_count;
typedef void(* sysclock_callback)(void);
static sysclock_callback sysclock_cb = NULL;
#define TICKS_PER_MS    (SystemCoreClock / 1000)
#define TICKS_PER_US    (SystemCoreClock / 1000000)

TaskPeriod_t tp;
volatile TaskPeriodFlag_t tpf;
void sysclock_callback_register(sysclock_callback cb)
{
	sysclock_cb = cb;
}
void Sysclock_Init(void)
{
	tp.period_1ms = 0;
	tp.period_5ms = 0;
	tp.period_10ms = 0;
	tp.period_20ms = 0;
	tp.period_100ms = 0;
	tp.period_200ms = 0;
	tp.period_500ms = 0;
	tp.period_1000ms = 0;

	tpf.task_period_1ms = 0;
	tpf.task_period_5ms = 0;
	tpf.task_period_10ms = 0;
	tpf.task_period_20ms = 0;
	tpf.task_period_100ms = 0;
	tpf.task_period_200ms = 0;
	tpf.task_period_500ms = 0;
	tpf.task_period_1000ms = 0;

	sysclock_callback_register(SysClock_Cb);
	SysTick->LOAD = TICKS_PER_MS;
	SysTick->VAL = 0x00;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
					SysTick_CTRL_TICKINT_Msk |
					SysTick_CTRL_ENABLE_Msk;
}
uint64_t sys_tick_now(void)
{
	uint64_t base_before, base_after;
	uint32_t current_value;
	uint32_t reload;

	reload = SysTick->LOAD;
	do{
		base_before = sys_tick_count;
		current_value = SysTick->VAL;
		base_after = sys_tick_count;
	}while(base_before != base_after);
	return base_before + (uint64_t)(reload - current_value);
}
uint64_t sysclock_get_us(void)
{
	return sys_tick_now()/TICKS_PER_US;
}

uint64_t sysclock_get_ms(void)
{
	return sys_tick_now()/TICKS_PER_MS;
}

void sys_delay_ms(uint32_t ms)
{
	uint64_t now = sys_tick_now();
	while(sys_tick_now() - now < (uint64_t)ms*TICKS_PER_MS);
}
void sys_delay_us(uint32_t us)
{
	uint64_t now = sys_tick_now();
	while(sys_tick_now() - now < (uint64_t)us*TICKS_PER_US);
}

void SysClock_Cb(void)
{	
	static volatile uint64_t now;
	now = sys_tick_now();
	if(now - tp.period_1ms >= 1 * TICKS_PER_MS)
	{
		tp.period_1ms = now;
		tpf.task_period_1ms=1;
	}
	if(now - tp.period_5ms >= 5 * TICKS_PER_MS)
	{
		tp.period_5ms = now;
		tpf.task_period_5ms=1;
	}
	if(now - tp.period_10ms >= 10 * TICKS_PER_MS)
	{
		tp.period_10ms = now;
		tpf.task_period_10ms=1;
	}
	if(now - tp.period_20ms >= 20 * TICKS_PER_MS)
	{
		tp.period_20ms = now;
		tpf.task_period_20ms=1;
	}
	if(now - tp.period_100ms >= 100 * TICKS_PER_MS)
	{
		tp.period_100ms = now;
		tpf.task_period_100ms=1;
	}
	if(now - tp.period_200ms >= 200 * TICKS_PER_MS)
	{
		tp.period_200ms = now;
		tpf.task_period_200ms=1;
	}
	if(now - tp.period_500ms >= 500 * TICKS_PER_MS)
	{
		tp.period_500ms = now;
		tpf.task_period_500ms=1;
	}
	if(now - tp.period_1000ms >= 1000 * TICKS_PER_MS)
	{
		tp.period_1000ms = now;
		tpf.task_period_1000ms=1;
	}

}
void SysTick_Handler(void)
{
	sys_tick_count+=TICKS_PER_MS;
	if(sysclock_cb != NULL)
	{
		sysclock_cb();
	}
}
