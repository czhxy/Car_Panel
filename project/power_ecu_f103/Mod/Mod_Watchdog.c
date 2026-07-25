#include "Mod_Watchdog.h"
#include "drv_iwdg.h"

void Mod_Watchdog_Init(void)
{
	Drv_IWDG_Init();
}

void Mod_Watchdog_Feed(void)
{
	Drv_IWDG_Feed();
}
