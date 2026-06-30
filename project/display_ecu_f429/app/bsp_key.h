#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp_log.h"
void BSP_KEY_Init(void);
/* 按键任务函数声明，供 main.c 中 xTaskCreate 引用 */
void prvKeyScanTask(void *pvParameters);
//项目暂时只用到K1 K2
//K1 K2按下接地
#define KEY1_Port GPIOE
#define KEY1_Pin GPIO_Pin_2

#define KEY2_Port GPIOI
#define KEY2_Pin GPIO_Pin_11

//K3 K4按下接3V3 PA0为唤醒按键
#define KEY3_Port GPIOA
#define KEY3_Pin GPIO_Pin_0

#define KEY4_Port GPIOC
#define KEY4_Pin GPIO_Pin_13

extern SemaphoreHandle_t xKey1Sem;
extern SemaphoreHandle_t xKey2Sem;
#endif
