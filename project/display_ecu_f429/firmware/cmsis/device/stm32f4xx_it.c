/**
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  *          故障处理器 + CAN1_RX0_IRQHandler 在此强定义（覆盖 startup WEAK）。
  *          FreeRTOS 的 SVC/PendSV/SysTick 由 port.c 经 FreeRTOSConfig.h 宏映射提供。
  *          故障处理器的汇编部分在 stm32f4xx_it_asm.s 中实现。
  */
#include "stm32f4xx_it.h"
#include <stdio.h>
#ifndef BOOTLOADER
#include "mod_comm_can.h"
#endif

void NMI_Handler(void) {}
void DebugMon_Handler(void) {}

/* C 级诊断：打印栈帧 + CFSR 三段逐位解码 + HFSR/MMFAR/BFAR ---- */
void Dump_Fault_Info(unsigned int *sp)
{
    unsigned int cfsr;
    printf("\r\n========== FAULT ==========\r\n");
    printf("R0 =0x%08X  R1 =0x%08X  R2 =0x%08X  R3 =0x%08X\r\n", sp[0],sp[1],sp[2],sp[3]);
    printf("R12=0x%08X  LR =0x%08X  PC =0x%08X  xPSR=0x%08X\r\n", sp[4],sp[5],sp[6],sp[7]);
    printf("HFSR =0x%08X\r\n", (unsigned int)SCB->HFSR);
    cfsr = (unsigned int)SCB->CFSR;
    printf("CFSR =0x%08X", cfsr);
    if (cfsr & 0x000000FFu) {
        if (cfsr & (1u<<0))  printf(" IACCVIOL");
        if (cfsr & (1u<<1))  printf(" DACCVIOL");
        if (cfsr & (1u<<3))  printf(" MUNSTKERR");
        if (cfsr & (1u<<4))  printf(" MSTKERR");
        if (cfsr & (1u<<7))  printf(" MMARVALID");
    }
    if (cfsr & 0x0000FF00u) {
        if (cfsr & (1u<<8))  printf(" IBUSERR");
        if (cfsr & (1u<<9))  printf(" PRECISERR");
        if (cfsr & (1u<<10)) printf(" IMPRECISERR");
        if (cfsr & (1u<<11)) printf(" UNSTKERR");
        if (cfsr & (1u<<12)) printf(" STKERR");
        if (cfsr & (1u<<15)) printf(" BFARVALID");
    }
    if (cfsr & 0xFFFF0000u) {
        if (cfsr & (1u<<16)) printf(" UNDEFINSTR");
        if (cfsr & (1u<<17)) printf(" INVSTATE");
        if (cfsr & (1u<<18)) printf(" INVPC");
        if (cfsr & (1u<<19)) printf(" NOCP");
        if (cfsr & (1u<<24)) printf(" UNALIGNED");
        if (cfsr & (1u<<25)) printf(" DIVBYZERO");
    }
    printf("\r\n");
    printf("MMFAR=0x%08X  BFAR =0x%08X\r\n", (unsigned int)SCB->MMFAR, (unsigned int)SCB->BFAR);
    printf("================================\r\n\r\n");
    while (1) {}
}

/* ---- CAN1 FIFO0 接收中断（仅 App 使用） ---- */
#ifndef BOOTLOADER
void CAN1_RX0_IRQHandler(void)
{
    Mod_Can_RxIRQHandler();
}
#endif
