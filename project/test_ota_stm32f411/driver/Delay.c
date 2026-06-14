#include "Delay.h"

void Delay_us(uint32_t xus)
{
	SysTick->LOAD=(SystemCoreClock/1000/1000)*xus;
	SysTick->VAL=0x00;
	SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_ENABLE_Msk;
	while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));	//等待计数到0
	SysTick->CTRL = ~SysTick_CTRL_ENABLE_Msk;				//关闭定时器
}

void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}
