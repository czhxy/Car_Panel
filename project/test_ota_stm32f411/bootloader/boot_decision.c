/**
  ******************************************************************************
  * @file    boot_decision.c
  * @brief   启动决策状态机 — 分区校验 / 回滚 / 活跃分区切换
  ******************************************************************************
  */

#include "boot_decision.h"
#include "boot_config.h"
#include "boot_jump.h"
#include "ota_params.h"
#include <stdio.h>

// g_ota_param 在 boot_main.c 中定义
extern ota_param_t g_ota_param;

// ===== 分区地址辅助 =====
uint32_t get_active_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_A_ADDR : APP_B_ADDR;
}

uint32_t get_inactive_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_B_ADDR : APP_A_ADDR;
}

uint32_t get_inactive_size(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? APP_B_SIZE : APP_A_SIZE;
}

// ===== 切换活跃分区 =====
void swap_active_partition(void)
{
    g_ota_param.active_partition =
        (g_ota_param.active_partition == APP_A_ACTIVE)
        ? APP_B_ACTIVE : APP_A_ACTIVE;
}

// ===== 回滚到旧分区 =====
int perform_rollback(void)
{
    printf("[BOOT] ROLLBACK triggered!\r\n");

    swap_active_partition();
    uint32_t old_addr = get_active_addr();

    printf("[BOOT] Rolling back to 0x%08X\r\n", (unsigned int)old_addr);

    if (!partition_is_valid(old_addr)) {
        printf("[BOOT] FATAL: Old partition also invalid!\r\n");
        printf("[BOOT] Entering safe mode (wait for upgrade)...\r\n");
        return -1;
    }

    g_ota_param.ota_state = OTA_STATE_IDLE;
    g_ota_param.boot_count = 0;
    ota_params_save(&g_ota_param);

    printf("[BOOT] Rollback OK.\r\n");
    return 0;
}

// ===== 启动决策状态机 =====
int boot_decision(void)
{
    switch (g_ota_param.ota_state) {

    case OTA_STATE_IDLE:
        g_ota_param.boot_count = 0;
        ota_params_save(&g_ota_param);

        if (partition_is_valid(get_active_addr())) {
            return 1;
        } else {
            printf("[BOOT] No valid app in active partition.\r\n");
            return 0;
        }

    case OTA_STATE_COMPLETE:
        {
            uint32_t new_addr = get_inactive_addr();

            g_ota_param.boot_count++;
            ota_params_save(&g_ota_param);

            printf("[BOOT] Boot attempt %u/%u\r\n",
                   g_ota_param.boot_count,
                   g_ota_param.max_boot_count);

            if (g_ota_param.boot_count > g_ota_param.max_boot_count) {
                printf("[BOOT] Max boot attempts exceeded.\r\n");
                if (perform_rollback() == 0) {
                    return 1;
                }
                return 0;
            }

            uint32_t new_size = (new_addr == APP_A_ADDR)
                                ? g_ota_param.app_a_size
                                : g_ota_param.app_b_size;

            if (new_size == 0 || new_size == 0xFFFFFFFF) {
                printf("[BOOT] Invalid new firmware size (0x%08X).\r\n",
                       (unsigned int)new_size);
                perform_rollback();
                return 1;
            }

            uint32_t calc_crc = crc32_flash(new_addr, new_size);
            uint32_t saved_crc = (new_addr == APP_A_ADDR)
                                 ? g_ota_param.app_a_crc32
                                 : g_ota_param.app_b_crc32;

            printf("[BOOT] CRC32 verify: saved=0x%08X calc=0x%08X\r\n",
                   (unsigned int)saved_crc, (unsigned int)calc_crc);

            if (calc_crc == saved_crc) {
                printf("[BOOT] New firmware verified, switching partition.\r\n");
                swap_active_partition();
                g_ota_param.ota_state = OTA_STATE_IDLE;
                g_ota_param.boot_count = 0;
                ota_params_save(&g_ota_param);
                return 1;
            } else {
                printf("[BOOT] CRC32 mismatch, rolling back.\r\n");
                perform_rollback();
                return 1;
            }
        }

    case OTA_STATE_FAILED:
        printf("[BOOT] Previous upgrade failed, rolling back.\r\n");
        perform_rollback();
        return 1;

    default:
        printf("[BOOT] Unknown OTA state %u, entering upgrade mode.\r\n",
               g_ota_param.ota_state);
        return 0;
    }
}
