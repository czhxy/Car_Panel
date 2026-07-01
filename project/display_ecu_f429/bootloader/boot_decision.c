/**
  ******************************************************************************
  * @file    boot_decision.c
  * @brief   启动决策状态机 — 校验 A 区 / 回滚 B→A / 进入 OTA
  *
  *          搭配 A→B 备份方案：A 区运行，B 区备份。
  *          新固件校验失败且超次数 → B→A 恢复备份。
  ******************************************************************************
  */

#include "boot_decision.h"
#include "boot_config.h"
#include "boot_jump.h"
#include "ota_params.h"
#include "flash_control.h"
#include <stdio.h>

// g_ota_param 在 boot_main.c 中定义
extern ota_param_t g_ota_param;

// ===== 分区地址辅助 =====
uint32_t get_active_addr(void)
{
    return APP_A_ADDR;
}

// ===== 从 B 区恢复备份到 A 区 =====
static int restore_backup(void)
{
    uint32_t bk_size  = g_ota_param.app_b_size;
    uint32_t bk_crc   = g_ota_param.app_b_crc32;

    if (bk_size == 0 || bk_size == 0xFFFFFFFF) {
        printf("[BOOT] No valid backup in B.\r\n");
        return -1;
    }

    // 先校验 B 区备份是否完整
    uint32_t crc_b = crc32_flash(APP_B_ADDR, bk_size);
    if (crc_b != bk_crc) {
        printf("[BOOT] Backup CRC mismatch (0x%08X vs 0x%08X).\r\n",
               (unsigned int)crc_b, (unsigned int)bk_crc);
        return -1;
    }

    printf("[BOOT] Restoring backup B->A (%u bytes)\r\n", (unsigned int)bk_size);
    flash_if_init();
    if (flash_if_erase(APP_A_ADDR, APP_A_SIZE) != 0) {
        printf("[BOOT] A region erase FAILED!\r\n");
        flash_if_lock();
        return -1;
    }
    if (flash_if_copy(APP_B_ADDR, APP_A_ADDR, bk_size) != 0) {
        printf("[BOOT] Restore copy FAILED!\r\n");
        flash_if_lock();
        return -1;
    }
    flash_if_lock();

    uint32_t crc_a = crc32_flash(APP_A_ADDR, bk_size);
    if (crc_a != bk_crc) {
        printf("[BOOT] Restore verification FAILED.\r\n");
        return -1;
    }

    // 恢复备份的固件信息到 app_a
    g_ota_param.app_a_version = g_ota_param.app_b_version;
    g_ota_param.app_a_size    = bk_size;
    g_ota_param.app_a_crc32   = bk_crc;
    printf("[BOOT] Restore OK.\r\n");
    return 0;
}

// ===== 启动决策状态机 =====
int boot_decision(void)
{
    switch (g_ota_param.ota_state) {

    case OTA_STATE_IDLE:
        if (partition_is_valid(APP_A_ADDR)) {
            return 1;
        } else {
            printf("[BOOT] No valid app in A partition.\r\n");
            return 0;
        }

    case OTA_STATE_COMPLETE:
        {
            g_ota_param.boot_count++;
            ota_params_save(&g_ota_param);

            printf("[BOOT] Boot attempt %u/%u\r\n",
                   g_ota_param.boot_count,
                   g_ota_param.max_boot_count);

            if (g_ota_param.app_a_size == 0 ||
                g_ota_param.app_a_size == 0xFFFFFFFF) {
                printf("[BOOT] Invalid firmware size (0x%08X).\r\n",
                       (unsigned int)g_ota_param.app_a_size);
                return 0;
            }

            uint32_t calc_crc = crc32_flash(APP_A_ADDR,
                                             g_ota_param.app_a_size);

            printf("[BOOT] CRC32: saved=0x%08X calc=0x%08X\r\n",
                   (unsigned int)g_ota_param.app_a_crc32,
                   (unsigned int)calc_crc);

            if (calc_crc == g_ota_param.app_a_crc32) {
                printf("[BOOT] Firmware verified OK.\r\n");
                g_ota_param.ota_state  = OTA_STATE_IDLE;
                g_ota_param.boot_count = 0;
                ota_params_save(&g_ota_param);
                return 1;
            }

            // CRC 失败，检查是否超限
            if (g_ota_param.boot_count >= g_ota_param.max_boot_count) {
                printf("[BOOT] Max boot attempts, restoring backup...\r\n");
                if (restore_backup() == 0) {
                    g_ota_param.ota_state  = OTA_STATE_IDLE;
                    g_ota_param.boot_count = 0;
                    ota_params_save(&g_ota_param);
                    return 1;
                }
                printf("[BOOT] Restore failed, entering OTA mode.\r\n");
                return 0;
            }

            // 未超限，等下次重启再试
            printf("[BOOT] CRC mismatch (attempt %u), retry next boot.\r\n",
                   g_ota_param.boot_count);
            return 0;
        }

    case OTA_STATE_FAILED:
        printf("[BOOT] Previous upgrade failed, entering OTA mode.\r\n");
        return 0;

    default:
        printf("[BOOT] Unknown OTA state %u, entering upgrade mode.\r\n",
               g_ota_param.ota_state);
        return 0;
    }
}
