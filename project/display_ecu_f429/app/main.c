/**
  ******************************************************************************
  * @file    main.c
  * @brief   display_ecu_f429 App 主程序 (App @ 0x08020000)
  ******************************************************************************
  */

#include "main.h"
#include "Delay.h"
#include "usart.h"
#include <stdio.h>

static volatile uint32_t g_timing_delay;

void SysTick_Handler(void)
{
    if (g_timing_delay) g_timing_delay--;
}

static void delay_ms(uint32_t ms)
{
    g_timing_delay = ms;
    while (g_timing_delay);
}

int main(void)
{
    SCB->VTOR = 0x08020000;

    SysTick_Config(SystemCoreClock / 1000);
    __enable_irq();

    UART_Init();
    LED_Init();

    printf("\r\n================================\r\n");
    printf("App v1.0 (STM32F429) @ 0x%08X\r\n", (unsigned int)SCB->VTOR);
    printf("SystemCoreClock = %u MHz\r\n",
           (unsigned int)(SystemCoreClock / 1000000));
    printf("================================\r\n\r\n");

    uint32_t tick = 0;
    while (1)
    {
        GPIO_ToggleBits(GPIOE, GPIO_Pin_3);
        GPIO_ToggleBits(GPIOH, GPIO_Pin_10);
        GPIO_ToggleBits(GPIOH, GPIO_Pin_11);
        GPIO_ToggleBits(GPIOH, GPIO_Pin_12);
        delay_ms(500);

        if (++tick % 2 == 0) {
            printf("[APP] heart=%u\r\n", tick / 2);
        }
    }
}
