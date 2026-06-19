/**
  ******************************************************************************
  * @file    bsp_led.c
  * @brief   LED 板级支持：PE3/PH10/PH11/PH12 推挽输出
  ******************************************************************************
  */

#include "bsp_led.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"

void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOH, ENABLE);

    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    gpio.GPIO_Pin = GPIO_Pin_3;
    GPIO_Init(GPIOE, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_Init(GPIOH, &gpio);
}
