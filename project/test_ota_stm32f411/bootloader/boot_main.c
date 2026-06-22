/**
  ******************************************************************************
  * @file    boot_main.c
  * @brief   Bootloader 入口 — 仅含 main() 主流程
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "boot_config.h"
#include "boot_decision.h"
#include "boot_jump.h"
#include "key.h"
#include "ota.h"
#include "ota_params.h"
#include "usart.h"
#include "delay.h"
#include <stdio.h>

// ===== 全局 OTA 参数 =====
ota_param_t g_ota_param;

int main(void)
{
    UART_Init();
    key_init();

    printf("\r\n================================\r\n");
    printf("Bootloader v1.0 (STM32F411)\r\n");
    printf("Flash: %dKB (0x%08X - 0x%08X)\r\n",
           (int)(FLASH_TOTAL_SIZE / 1024),
           (unsigned int)FLASH_BASE_ADDR,
           (unsigned int)(FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - 1));
    printf("================================\r\n\r\n");

    // ===== 加载 / 初始化 OTA 参数 =====
    ota_params_load(&g_ota_param);

    if (g_ota_param.magic != OTA_MAGIC) {
        printf("[BOOT] OTA param magic mismatch (0x%08X), initializing...\r\n",
               (unsigned int)g_ota_param.magic);
        if (ota_params_init() != 0) {
            printf("[BOOT] OTA param init FAILED!\r\n");
            while (1);
        }
        ota_params_load(&g_ota_param);
        printf("[BOOT] OTA param initialized.\r\n");
    } else {
        printf("[BOOT] OTA param loaded OK.\r\n");
    }

    printf("[BOOT] Active partition: %s\r\n",
           g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B");
    printf("[BOOT] OTA state: %u\r\n", g_ota_param.ota_state);
    printf("[BOOT] App A ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_a_version,
           (unsigned int)g_ota_param.app_a_size);
    printf("[BOOT] App B ver: 0x%08X, size: %u\r\n",
           (unsigned int)g_ota_param.app_b_version,
           (unsigned int)g_ota_param.app_b_size);

    // ===== 启动决策状态机 =====
    int should_jump = boot_decision();

    if (should_jump) {
        uint32_t addr = get_active_addr();
        if (partition_is_valid(addr)) {
            printf("[BOOT] App found at 0x%08X (%s).\r\n",
                   (unsigned int)addr,
                   (g_ota_param.active_partition == APP_A_ACTIVE)
                       ? "App A" : "App B");
            printf("[BOOT] OTA upgrade: press PA0 button within 2s...\r\n");

            if (key_wait_press(2000)) {
                printf("[BOOT] BTN pressed, entering OTA mode.\r\n");
            } else {
                printf("[BOOT] Timeout, jumping to App...\r\n");
                jump_to_app(addr);
            }
        }
    }

    // ===== 未能跳转或用户选择升级, 直接启动 OTA =====
    printf("[BOOT] Entering upgrade mode, starting YMODEM...\r\n");
    ota_ymodem_start();
    while (1) {
        printf("[BOOT] OTA failed, retry in 3s...\r\n");
        Delay_ms(3000);
        ota_ymodem_start();
    }
}
