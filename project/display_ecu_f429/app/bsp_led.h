#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f4xx.h"

void BSP_LED_Init(void);

/* LED 任务函数声明，供 main.c 中 xTaskCreate 引用 */
void LEDTask1(void * pvParameters);
void LEDTask2(void * pvParameters);
void LEDTask3(void * pvParameters);
#define LED1_Port GPIOH
#define LED1_Pin GPIO_Pin_12

#define LED2_Port GPIOH
#define LED2_Pin GPIO_Pin_10

#define LED3_Port GPIOH
#define LED3_Pin GPIO_Pin_11

#define LED4_Port GPIOE
#define LED4_Pin GPIO_Pin_3
#endif
