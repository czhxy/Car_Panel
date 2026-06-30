#include "main.h"

extern void Task_Entry_All(void * pvParameters);

int main(void)
{
    /* 通过 __Vectors 符号动态获取当前 App 的矢量表基址，
     * 无论运行在 App A (0x08020000) 还是 App B (0x08080000) 都自动适配 */
    extern uint32_t __Vectors;
    SCB->VTOR = (uint32_t)&__Vectors;

    /* NVIC 优先级分组: 4 位全用于抢占 (0-15)，对 FreeRTOS 和 OTA 串口中断至关重要 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* SysTick 由 FreeRTOS 的 vPortSetupTimerInterrupt() 自动配置，此处不手动调用 */
    __enable_irq();
    UART_Init();
    LOG_I("\r\n================================\r\n");
    LOG_I("App v%d.%d (STM32F429) @ 0x%08X\r\n",
           (int)APP_VERSION, (int)((APP_VERSION - (int)APP_VERSION) * 10 + 0.5),
           (unsigned int)SCB->VTOR);
    LOG_I("SystemCoreClock = %u MHz\r\n",
           (unsigned int)(SystemCoreClock / 1000000));
    LOG_I("================================\r\n\r\n");

    if(xTaskCreate(Task_Entry_All, "ALL_Task_Entry", 256, NULL, 30, NULL) != pdPASS)
        LOG_E("[Main] ALL_Task_Entry create failed!\r\n");
		vTaskStartScheduler();
    while (1)
    {
		
    }
}
