#include "bsp_key.h"
SemaphoreHandle_t xKey1Sem = NULL;
SemaphoreHandle_t xKey2Sem = NULL;
static uint8_t sPrevKey1State = 1;
static uint8_t sPrevKey2State = 1;
static uint8_t sCurrKey1State = 1;
static uint8_t sCurrKey2State = 1;
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
	
	if((xKey1Sem = xSemaphoreCreateBinary()) == NULL)
	{
		LOG_E("[Key] Key1 create failed!\r\n");
	}
	if((xKey2Sem = xSemaphoreCreateBinary()) == NULL)
	{
		LOG_E("[Key] Key2 create failed!\r\n");
	}
}

/**
 * @brief  按键扫描任务
 */
void prvKeyScanTask(void * pvParameters)
{
	(void)pvParameters;
	while (1)
	{
		sCurrKey1State = GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin);
		sCurrKey2State = GPIO_ReadInputDataBit(KEY2_Port, KEY2_Pin);
		if (sCurrKey1State == Bit_SET && sPrevKey1State == Bit_RESET) {
				xSemaphoreGive(xKey1Sem);
		}
		if (sCurrKey2State == Bit_SET && sPrevKey2State == Bit_RESET) {
				xSemaphoreGive(xKey2Sem);
		}
		sPrevKey1State = sCurrKey1State;
		sPrevKey2State = sCurrKey2State;
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
