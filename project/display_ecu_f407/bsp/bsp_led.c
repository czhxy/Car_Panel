#include "bsp_led.h"
#include "bsp_key.h"
#include "FreeRTOS.h"
#include "task.h"

void BSP_LED_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = LED1_Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(LED1_Port, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = LED2_Pin;
	GPIO_Init(LED2_Port, &GPIO_InitStructure);
}

void LEDTask1(void * pvParameters)
{
	(void)pvParameters;
	uint8_t blink_on = 0;  /* 0=熄灭, 1=闪烁 */

	GPIO_WriteBit(LED1_Port, LED1_Pin, Bit_SET);  /* 初始熄灭 */

	while (1)
	{
		/* 按键翻转闪烁状态 */
		if (BSP_Key_GetState(1) != 0)
		{
			blink_on = !blink_on;
			if (blink_on == 0)
			{
				GPIO_WriteBit(LED1_Port, LED1_Pin, Bit_SET);  /* 立即灭 */
			}
		}

		if (blink_on)
		{
			GPIO_WriteBit(LED1_Port, LED1_Pin, Bit_RESET);  /* 亮 */
			vTaskDelay(pdMS_TO_TICKS(500));
			GPIO_WriteBit(LED1_Port, LED1_Pin, Bit_SET);    /* 灭 */
			vTaskDelay(pdMS_TO_TICKS(500));
		}
		else
		{
			vTaskDelay(pdMS_TO_TICKS(10));  /* 熄灭时仅短暂等待，及时响应按键 */
		}
	}
}

void LEDTask2(void * pvParameters)
{
	(void)pvParameters;
	uint8_t blink_on = 0;  /* 0=熄灭, 1=闪烁 */

	GPIO_WriteBit(LED2_Port, LED2_Pin, Bit_SET);  /* 初始熄灭 */

	while (1)
	{
		/* 按键翻转闪烁状态 */
		if (BSP_Key_GetState(2) != 0)
		{
			blink_on = !blink_on;
			if (blink_on == 0)
			{
				GPIO_WriteBit(LED2_Port, LED2_Pin, Bit_SET);  /* 立即灭 */
			}
		}

		if (blink_on)
		{
			GPIO_WriteBit(LED2_Port, LED2_Pin, Bit_RESET);  /* 亮 */
			vTaskDelay(pdMS_TO_TICKS(300));
			GPIO_WriteBit(LED2_Port, LED2_Pin, Bit_SET);    /* 灭 */
			vTaskDelay(pdMS_TO_TICKS(300));
		}
		else
		{
			vTaskDelay(pdMS_TO_TICKS(10));  /* 熄灭时仅短暂等待，及时响应按键 */
		}
	}
}
