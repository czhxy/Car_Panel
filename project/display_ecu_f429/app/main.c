#include "main.h"

extern void Task_Entry_All(void * pvParameters);


int main(void)
{
    SCB->VTOR = 0x08020000;

    /* SysTick 由 FreeRTOS 的 vPortSetupTimerInterrupt() 自动配置，此处不手动调用 */
    __enable_irq();
    UART_Init();
    printf("\r\n================================\r\n");
    printf("App v1.0 (STM32F429) @ 0x%08X\r\n", (unsigned int)SCB->VTOR);
    printf("SystemCoreClock = %u MHz\r\n",
           (unsigned int)(SystemCoreClock / 1000000));
    printf("================================\r\n\r\n");

    if(xTaskCreate(Task_Entry_All, "ALL_Task_Entry", 256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] ALL_Task_Entry create failed!\r\n");
    while (1)
    {

    }
}
