#include "bsp_key.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static SemaphoreHandle_t KEY1_Binary = NULL;
static SemaphoreHandle_t KEY2_Binary = NULL;

/* 按键消抖状态位，0=未触发，1=已触发（防重复） */
static uint8_t key1_debounce = 0;
static uint8_t key2_debounce = 0;

void BSP_KEY_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOI, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = KEY1_Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(KEY1_Port, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = KEY2_Pin;
	GPIO_Init(KEY2_Port, &GPIO_InitStructure);

	KEY1_Binary = xSemaphoreCreateBinary();
	KEY2_Binary = xSemaphoreCreateBinary();
}

/**
 * @brief  按键扫描任务，每 10ms 检测一次
 * @note   按下瞬间即给信号量，消抖机制避免重复触发
 */
void KEYTask(void * pvParameters)
{
	(void)pvParameters;

	while (1)
	{
		/* KEY1 检测：按下触发，释放复位 */
		if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET)
		{
			if (key1_debounce == 0)
			{
				vTaskDelay(pdMS_TO_TICKS(20));
				if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET)
				{
					key1_debounce = 1;
					xSemaphoreGive(KEY1_Binary);
				}
			}
		}
		else
		{
			key1_debounce = 0;  /* 释放后清零，允许下次触发 */
		}

		/* KEY2 检测：按下触发，释放复位 */
		if (GPIO_ReadInputDataBit(KEY2_Port, KEY2_Pin) == Bit_RESET)
		{
			if (key2_debounce == 0)
			{
				vTaskDelay(pdMS_TO_TICKS(20));
				if (GPIO_ReadInputDataBit(KEY2_Port, KEY2_Pin) == Bit_RESET)
				{
					key2_debounce = 1;
					xSemaphoreGive(KEY2_Binary);
				}
			}
		}
		else
		{
			key2_debounce = 0;  /* 释放后清零，允许下次触发 */
		}

		vTaskDelay(pdMS_TO_TICKS(10));  /* 扫描周期 */
	}
}

uint8_t BSP_Key_GetState(uint8_t id)
{
	if (id == 1)
	{
		if (xSemaphoreTake(KEY1_Binary, 0) == pdTRUE)
			return 1;
	}
	if (id == 2)
	{
		if (xSemaphoreTake(KEY2_Binary, 0) == pdTRUE)
			return 1;
	}
	return 0;
}
