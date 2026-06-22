/**
  ******************************************************************************
  * @file    boot_jump.h
  * @brief   Bootloader App 跳转 — 关中断/设 MSP/设 VTOR/跳转
  ******************************************************************************
  */

#ifndef __BOOT_JUMP_H
#define __BOOT_JUMP_H

#include "stm32f4xx.h"

void jump_to_app(uint32_t app_addr);
int  partition_is_valid(uint32_t addr);

#endif /* __BOOT_JUMP_H */
