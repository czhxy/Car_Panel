/**
  ******************************************************************************
  * @file    ota.c
  * @brief   OTA 升级入口 — A→B 备份 → YMODEM 烧录 A → CRC32 → 参数保存 → 重启
  *
  *          方案：A 区始终运行，B 区备份。
  *          OTA 前先 A→B 备份，再直接烧录 A 区，失败可回滚。
  ******************************************************************************
  */

#include "ota.h"
#include "boot_config.h"
#include "boot_decision.h"
#include "boot_jump.h"
#include "ota_params.h"
#include "flash_control.h"
#include "ymodem.h"
#include "Delay.h"
#include <stdio.h>

// g_ota_param 在 boot_main.c 中定义
extern ota_param_t g_ota_param;

void ota_ymodem_start(void)
{
    printf("[BOOT] Starting YMODEM OTA...\r\n");

    // ==== 第一步：A→B 备份当前固件 ====
    if (partition_is_valid(APP_A_ADDR)) {
        uint32_t old_size = (g_ota_param.app_a_size > 0 &&
                             g_ota_param.app_a_size < APP_A_SIZE)
                            ? g_ota_param.app_a_size : APP_A_SIZE;

        printf("[BOOT] Backing up A (0x%08X, %u bytes) -> B\r\n",
               (unsigned int)APP_A_ADDR, (unsigned int)old_size);

        flash_if_init();
        if (flash_if_erase(APP_B_ADDR, APP_B_SIZE) != 0) {
            printf("[BOOT] B region erase FAILED!\r\n");
            flash_if_lock();
            return;
        }
        // flash_if_copy 内 flash_if_write_word 已逐字验证写入，
        // 若返回 0 则 B 区数据与 A 区完全一致，无需额外 CRC 校验
        if (flash_if_copy(APP_A_ADDR, APP_B_ADDR, old_size) != 0) {
            printf("[BOOT] Backup copy FAILED!\r\n");
            flash_if_lock();
            return;
        }
        flash_if_lock();

        // 用实时 CRC 更新参数
        uint32_t crc = crc32_flash(APP_A_ADDR, old_size);
        g_ota_param.app_a_size    = old_size;
        g_ota_param.app_a_crc32   = crc;
        g_ota_param.app_b_version = g_ota_param.app_a_version;
        g_ota_param.app_b_size    = old_size;
        g_ota_param.app_b_crc32   = crc;
        // 立即持久化备份参数：即使后续 YMODEM 失败，B 区备份仍可恢复
        ota_params_save(&g_ota_param);
        printf("[BOOT] Backup OK (size=%u, CRC=0x%08X).\r\n",
               (unsigned int)old_size, (unsigned int)crc);
    } else {
        g_ota_param.app_b_size  = 0;
        g_ota_param.app_b_crc32 = 0;
        ota_params_save(&g_ota_param);
        printf("[BOOT] No existing firmware, skip backup.\r\n");
    }

    // ==== 第二步：YMODEM 直接烧录 A 区 ====
    printf("[BOOT] Target: 0x%08X (%uKB)\r\n",
           (unsigned int)APP_A_ADDR,
           (unsigned int)(APP_A_SIZE / 1024));

    ymodem_status_t status;
    int ret = ymodem_receive(APP_A_ADDR, APP_A_SIZE, &status);

    if (ret == YMODEM_OK) {
        printf("[BOOT] YMODEM transfer OK, verifying...\r\n");

        uint32_t crc = crc32_flash(APP_A_ADDR, status.total_received);
        printf("[BOOT] CRC32: 0x%08X\r\n", (unsigned int)crc);

        // 保存新固件 + 备份信息
        g_ota_param.active_partition = APP_A_ACTIVE;
        g_ota_param.ota_state        = OTA_STATE_COMPLETE;
        g_ota_param.app_a_version    = 0x00010001;
        g_ota_param.app_a_size       = status.total_received;
        g_ota_param.app_a_crc32      = crc;
        ota_params_save(&g_ota_param);

        printf("[BOOT] OTA params updated. Rebooting...\r\n");
        Delay_ms(500);
        NVIC_SystemReset();
    } else {
        printf("[BOOT] YMODEM failed (code: %d).\r\n", ret);
        // 烧录失败，B 区备份仍在，下次启动仍可跑旧固件
    }
}
