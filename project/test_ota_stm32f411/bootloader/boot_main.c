/**
  ******************************************************************************
  * @file    boot_main.c
  * @brief   Bootloader 入口 — YMODEM OTA + 双分区 A/B 回滚
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "boot_config.h"
#include "usart.h"
#include <stdio.h>

// ===== SysTick 轮询延时（Bootloader 中不使用中断） =====
static void delay_ms(uint32_t ms)
{
    SysTick->LOAD = 100000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    for (uint32_t i = 0; i < ms; i++) {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
    }
    SysTick->CTRL = 0;
}

// ===== 跳转到 App =====
typedef void (*app_entry_t)(void);

static void jump_to_app(uint32_t app_addr)
{
    uint32_t sp = *((volatile uint32_t *)app_addr);
    uint32_t pc = *((volatile uint32_t *)(app_addr + 4));

    printf("[BOOT] Jumping to App at 0x%08X...\r\n", (unsigned int)app_addr);
    printf("[BOOT]   SP = 0x%08X, PC = 0x%08X\r\n", (unsigned int)sp, (unsigned int)pc);

    if ((sp & 0x2FFE0000) != 0x20000000) {
        printf("[BOOT] ERROR: Invalid stack pointer! Abort jump.\r\n");
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

// ===== 检查 App 分区是否有效 =====
static int partition_is_valid(uint32_t addr)
{
    uint32_t sp = *((volatile uint32_t *)addr);
    return ((sp & 0x2FFE0000) == 0x20000000);
}

// =====================================================================
//  main - Bootloader 入口
// =====================================================================
int main(void)
{
    UART_Init();
    printf("\r\n================================\r\n");
    printf("Bootloader v1.0 (STM32F411)\r\n");
    printf("Flash: %dKB (0x%08X - 0x%08X)\r\n",
           (int)(FLASH_TOTAL_SIZE / 1024),
           (unsigned int)FLASH_BASE_ADDR,
           (unsigned int)(FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - 1));
    printf("================================\r\n\r\n");

    // ===== 阶段一：检查 App A 是否有效 =====
    uint32_t app_a_pc = *((volatile uint32_t *)(APP_A_ADDR + 4));

    printf("[BOOT] App A @ 0x%08X: PC=0x%08X\r\n",
           (unsigned int)APP_A_ADDR,
           (unsigned int)app_a_pc);

    if (partition_is_valid(APP_A_ADDR) && app_a_pc != 0xFFFFFFFF) {
        printf("[BOOT] App A looks valid, jumping...\r\n");
        delay_ms(500);
        jump_to_app(APP_A_ADDR);
    } else {
        printf("[BOOT] App A invalid, staying in bootloader.\r\n");
        printf("[BOOT] Waiting for YMODEM upgrade...\r\n");
    }

    // ===== 停留在 Bootloader，等待升级 =====
    while (1) {
        // TODO: 后续阶段——OTA 参数加载、YMODEM 接收、状态机
        delay_ms(1000);
        printf("[BOOT] Waiting...\r\n");
    }
}
