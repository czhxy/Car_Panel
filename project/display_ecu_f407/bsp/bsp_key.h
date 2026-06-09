#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "stm32f4xx.h"

void BSP_KEY_Init(void);
uint8_t BSP_Key_GetState(uint8_t id);
/* 按键任务函数声明，供 main.c 中 xTaskCreate 引用 */
void KEYTask(void *pvParameters);

#define KEY1_Port GPIOE
#define KEY1_Pin GPIO_Pin_3

#define KEY2_Port GPIOE
#define KEY2_Pin GPIO_Pin_4

#endif
