#ifndef __DRV_IWDG_H
#define __DRV_IWDG_H

#include "stm32f10x.h"

void Drv_IWDG_Init(void);       /* 初始化 IWDG，~1s 超时 */
void Drv_IWDG_Feed(void);       /* 喂狗，主循环调用 */

#endif
