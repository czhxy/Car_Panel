/**
  ******************************************************************************
  * @file    ota.c
  * @brief   OTA 升级入口 — YMODEM 接收 → CRC32 校验 → 参数保存 → 重启
  ******************************************************************************
  */

#include "ota.h"
#include "boot_config.h"
#include "boot_decision.h"
#include "ota_params.h"
#include "ymodem.h"
#include "Delay.h"
#include <stdio.h>

// g_ota_param 在 boot_main.c 中定义
extern ota_param_t g_ota_param;

void ota_ymodem_start(void)
{
    printf("[BOOT] Starting YMODEM OTA...\r\n");

    uint32_t target_addr = get_inactive_addr();
    uint32_t target_size = get_inactive_size();

    printf("[BOOT] Target: 0x%08X (%uKB)\r\n",
           (unsigned int)target_addr,
           (unsigned int)(target_size / 1024));

    ymodem_status_t status;
    int ret = ymodem_receive(target_addr, target_size, &status);

    if (ret == YMODEM_OK) {
        printf("[BOOT] YMODEM transfer OK, verifying...\r\n");

        uint32_t crc = crc32_flash(target_addr, status.total_received);
        printf("[BOOT] CRC32: 0x%08X\r\n", (unsigned int)crc);

        g_ota_param.ota_state = OTA_STATE_COMPLETE;
        if (target_addr == APP_B_ADDR) {
            g_ota_param.app_b_version = 0x00010001;
            g_ota_param.app_b_size    = status.total_received;
            g_ota_param.app_b_crc32   = crc;
        } else {
            g_ota_param.app_a_version = 0x00010001;
            g_ota_param.app_a_size    = status.total_received;
            g_ota_param.app_a_crc32   = crc;
        }
        ota_params_save(&g_ota_param);

        printf("[BOOT] OTA params updated. Rebooting...\r\n");
        Delay_ms(500);
        NVIC_SystemReset();
    } else {
        printf("[BOOT] YMODEM failed (code: %d).\r\n", ret);
    }
}
