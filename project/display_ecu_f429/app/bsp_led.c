#include "bsp_led.h"
#include "FreeRTOS.h"
#include "task.h"

void BSP_LED_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOH, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = LED1_Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(LED1_Port, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LED2_Pin;
	GPIO_Init(LED2_Port, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LED3_Pin;
	GPIO_Init(LED3_Port, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LED4_Pin;
	GPIO_Init(LED4_Port, &GPIO_InitStructure);
	
	GPIO_SetBits(LED1_Port,LED1_Pin);
	GPIO_SetBits(LED2_Port,LED2_Pin);
	GPIO_SetBits(LED3_Port,LED3_Pin);
	GPIO_SetBits(LED4_Port,LED4_Pin);
}

