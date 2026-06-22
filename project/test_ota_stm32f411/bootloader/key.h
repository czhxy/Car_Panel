/**
  ******************************************************************************
  * @file    key.h
  * @brief   Bootloader 按键管理 — PA0 (按下低电平)
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx.h"

void key_init(void);
int  key_is_pressed(void);
int  key_wait_press(uint32_t timeout_ms);

#endif /* __KEY_H */
