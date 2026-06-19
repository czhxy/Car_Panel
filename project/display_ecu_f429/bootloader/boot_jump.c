/**
  ******************************************************************************
  * @file    boot_jump.c
  * @brief   Bootloader App 跳转 — 关中断/清除挂起/设 MSP/设 VTOR/跳转
  *          STM32F429: SRAM 192KB (0x20000000 - 0x2002FFFF)
  ******************************************************************************
  */

#include "boot_jump.h"
#include <stdio.h>

typedef void (*app_entry_t)(void);

void jump_to_app(uint32_t app_addr)
{
    uint32_t sp = *((volatile uint32_t *)app_addr);
    uint32_t pc = *((volatile uint32_t *)(app_addr + 4));

    printf("[BOOT] Jumping to App at 0x%08X...\r\n", (unsigned int)app_addr);
    printf("[BOOT]   SP = 0x%08X, PC = 0x%08X\r\n", (unsigned int)sp, (unsigned int)pc);

    // F429 有 192KB SRAM (0x20000000 - 0x20030000)
    if (sp < 0x20000000 || sp >= 0x20030000) {
        printf("[BOOT] ERROR: Invalid stack pointer 0x%08X! Abort jump.\r\n", (unsigned int)sp);
        return;
    }

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = app_addr & 0xFFFFFF00;
    __set_MSP(sp);

    app_entry_t entry = (app_entry_t)pc;
    entry();
}

int partition_is_valid(uint32_t addr)
{
    uint32_t sp = *((volatile uint32_t *)addr);
    // F429: 192KB SRAM @ 0x20000000
    return (sp >= 0x20000000 && sp < 0x20030000);
}
