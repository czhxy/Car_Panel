#include "main.h"

int main()
{
	/* ---- 外设初始化 ----
	 * BSP_USART_Init 内部已经创建了 xUartRxSem、初始化 lwrb 并启动 DMA。 */
	BSP_USART_Init();
	BSP_LED_Init();
	BSP_KEY_Init();

	SafePrintf("FreeRTOS Template (lwrb)\r\n");

	/* ---- 创建 FreeRTOS 任务 ---- */
	if (xUartRxSem != NULL)
	{
		xTaskCreate(UARTRxTask, "UART_RX", 256, NULL, 2, NULL);
	}

	xTaskCreate(LEDTask1,  "LED1", 256, NULL, 1, NULL);
	xTaskCreate(LEDTask2,  "LED2", 256, NULL, 1, NULL);
	xTaskCreate(KEYTask,   "KEY",  256, NULL, 2, NULL);

	/* ---- 启动调度器 ---- */
	vTaskStartScheduler();
	while (1)
	{

	}
}
