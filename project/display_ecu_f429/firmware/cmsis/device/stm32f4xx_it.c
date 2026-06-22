/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/stm32f4xx_it.c
  * @author  MCD Application Team
  * @version V1.8.1
  * @date    27-January-2022
  * @brief   Main Interrupt Service Routines.
  *
  *          故障处理器 (HardFault/MemManage/BusFault/UsageFault)
  *          已移至 app/main.c 强定义（带串口诊断输出）。
  *          startup 文件中保留 WEAK 默认（死循环）作为兜底。
  ******************************************************************************
  */

#include "stm32f4xx_it.h"

void NMI_Handler(void)
{
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

// SysTick_Handler 在各自 main 文件中定义
