/**
  ******************************************************************************
  * @file    key.c
  * @brief   Bootloader 按键管理 — PA0, 按下为低电平
  ******************************************************************************
  */

#include "key.h"
#include "delay.h"

#define KEY_PORT    GPIOA
#define KEY_PIN     GPIO_Pin_0
#define KEY_PRESSED 0       /* 按下时引脚为低电平 */

void key_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef g;
    GPIO_StructInit(&g);
    g.GPIO_Mode = GPIO_Mode_IN;
    g.GPIO_Pin  = KEY_PIN;
    g.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(KEY_PORT, &g);
}

int key_is_pressed(void)
{
    return (GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN) == KEY_PRESSED);
}

int key_wait_press(uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if (key_is_pressed())
            return 1;
        Delay_ms(1);
    }
    return 0;
}
