#include "bsp_key.h"
SemaphoreHandle_t xBinarySemKey1 = NULL;
SemaphoreHandle_t xBinarySemKey2 = NULL;

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
	
	if((xBinarySemKey1 = xSemaphoreCreateBinary()) == NULL)
	{
		LOG_E("[Key] Key1 create failed!\r\n");
	}
	if((xBinarySemKey2 = xSemaphoreCreateBinary()) == NULL)
	{
		LOG_E("[Key] Key2 create failed!\r\n");
	}
}

/**
 * @brief  按键扫描任务
 */
void KEYTask(void * pvParameters)
{
	(void)pvParameters;
	uint8_t debounce1 = 0;
	uint8_t debounce2 = 0;
	
	while (1)
	{	
		if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET) {
            if (debounce1 == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));
                if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET) {
                    debounce1 = 1;
                    xSemaphoreGive(xBinarySemKey1);
                }
            }
        } else {
            debounce1 = 0;
        }
        
		/* KEY2 检测：按下时打印状态 */
		if (GPIO_ReadInputDataBit(KEY2_Port, KEY2_Pin) == Bit_RESET) {
            if (debounce2 == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));
                if (GPIO_ReadInputDataBit(KEY2_Port, KEY2_Pin) == Bit_RESET) {
                    debounce2 = 1;
                    xSemaphoreGive(xBinarySemKey2);
                }
            }
        } else {
            debounce2 = 0;
        }

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
