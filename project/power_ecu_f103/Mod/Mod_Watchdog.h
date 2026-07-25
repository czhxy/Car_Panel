#ifndef __MOD_WATCHDOG_H
#define __MOD_WATCHDOG_H

#include "stm32f10x.h"

void Mod_Watchdog_Init(void);    /* 初始化 IWDG，~1s 超时 */
void Mod_Watchdog_Feed(void);    /* 喂狗 */

#endif
