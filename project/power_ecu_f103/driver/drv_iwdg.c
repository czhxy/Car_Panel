#include "drv_iwdg.h"

/**
 * Drv_IWDG_Init — 初始化独立看门狗
 *
 * F103 LSI ≈ 40kHz。IWDG 时钟 = LSI / Prescaler。
 * Prescaler=256 → 40k/256 ≈ 156.25Hz, 每 tick ≈ 6.4ms。
 * Reload=156 → 超时 ≈ 156 × 6.4ms ≈ 1s。
 */
void Drv_IWDG_Init(void)
{
	/* 使能 LSI 并等待就绪 */
	RCC_LSICmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

	/* 使能写访问 */
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	/* 预分频 256 */
	IWDG_SetPrescaler(IWDG_Prescaler_256);

	/* 重装载值 156 → ~1s 超时 */
	IWDG_SetReload(156);

	/* 加载重装载值并启动 */
	IWDG_ReloadCounter();
	IWDG_Enable();
}

/**
 * Drv_IWDG_Feed — 喂狗
 * 必须在主循环中周期调用（<1s 间隔）
 */
void Drv_IWDG_Feed(void)
{
	IWDG_ReloadCounter();
}
