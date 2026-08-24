/**
  ******************************************************************************
  * @file    boot_decision.c
  * @brief   启动决策状态机 — 真 AB：按 active_partition 选址 / 切槽回滚 / 进入 OTA
  *
  *          真 AB 方案：A、B 两槽各存一份完整镜像，bootloader 按 active_partition
  *          跳转活跃槽。新固件 OTA 写入【非活跃槽】并翻转 active；旧固件仍留在
  *          原活跃槽，作为回滚目标。故回滚 = 切回另一槽，无需 384KB Flash 搬运。
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

// ===== 当前活跃槽地址（按 active_partition 选择） =====
uint32_t get_active_addr(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE) ? APP_A_ADDR : APP_B_ADDR;
}

// ===== 当前活跃槽的元数据 =====
static uint32_t active_size(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? g_ota_param.app_a_size : g_ota_param.app_b_size;
}
static uint32_t active_crc(void)
{
    return (g_ota_param.active_partition == APP_A_ACTIVE)
           ? g_ota_param.app_a_crc32 : g_ota_param.app_b_crc32;
}

// ===== 回滚：切到另一槽（真 AB 无需 Flash 搬运） =====
static int rollback_to_other(void)
{
    uint8_t  other  = (g_ota_param.active_partition == APP_A_ACTIVE)
                      ? APP_B_ACTIVE : APP_A_ACTIVE;
    uint32_t o_addr = (other == APP_A_ACTIVE) ? APP_A_ADDR : APP_B_ADDR;
    uint32_t o_size = (other == APP_A_ACTIVE)
                      ? g_ota_param.app_a_size : g_ota_param.app_b_size;
    uint32_t o_crc  = (other == APP_A_ACTIVE)
                      ? g_ota_param.app_a_crc32 : g_ota_param.app_b_crc32;

    printf("[BOOT] Rollback candidate: %s.\r\n",
           other == APP_A_ACTIVE ? "App A" : "App B");

    if (!partition_is_valid(o_addr)) {
        printf("[BOOT] Other slot SP invalid, cannot roll back.\r\n");
        return -1;
    }

    if (o_size == 0 || o_size == 0xFFFFFFFF) {
        // 另一槽无元数据（如出厂首烧的槽）-> 仅凭 SP 合法性信任
        printf("[BOOT] Other slot has no metadata, trust by SP.\r\n");
    } else {
        uint32_t calc = crc32_flash(o_addr, o_size);
        if (calc != o_crc) {
            printf("[BOOT] Other slot CRC mismatch (0x%08X vs 0x%08X).\r\n",
                   (unsigned int)calc, (unsigned int)o_crc);
            return -1;
        }
    }

    g_ota_param.active_partition = other;
    printf("[BOOT] Rollback to %s.\r\n",
           other == APP_A_ACTIVE ? "App A" : "App B");
    return 0;
}

// ===== 启动决策状态机 =====
int boot_decision(void)
{
    switch (g_ota_param.ota_state) {

    case OTA_STATE_IDLE:
        if (partition_is_valid(get_active_addr())) {
            return 1;
        }
        printf("[BOOT] Active slot invalid.\r\n");
        return 0;

    case OTA_STATE_COMPLETE:
        {
            g_ota_param.boot_count++;
            ota_params_save(&g_ota_param);

            printf("[BOOT] Boot attempt %u/%u\r\n",
                   g_ota_param.boot_count,
                   g_ota_param.max_boot_count);

            uint32_t addr = get_active_addr();
            uint32_t size = active_size();
            uint32_t crc  = active_crc();

            if (size == 0 || size == 0xFFFFFFFF) {
                printf("[BOOT] Active slot has no metadata, entering OTA.\r\n");
                return 0;
            }

            uint32_t calc_crc = crc32_flash(addr, size);
            printf("[BOOT] CRC32: saved=0x%08X calc=0x%08X\r\n",
                   (unsigned int)crc, (unsigned int)calc_crc);

            /* 先判断启动失败次数是否已超过最大上限：超限则直接切槽回滚，
             * 不再校验 CRC。这样能覆盖"新固件 CRC 正确但启动即崩溃"的窗口
             * —— App 崩 → IWDG 复位 → 再次进入本分支 → boot_count 递增 →
             * 达上限即回滚，而非无限重试。 */
            if (g_ota_param.boot_count >= g_ota_param.max_boot_count) {
                printf("[BOOT] Max boot attempts, rolling back to other slot...\r\n");
                if (rollback_to_other() == 0) {
                    g_ota_param.ota_state  = OTA_STATE_IDLE;
                    g_ota_param.boot_count = 0;
                    ota_params_save(&g_ota_param);
                    return 1;
                }
                printf("[BOOT] Rollback failed, entering OTA mode.\r\n");
                return 0;
            }

            if (calc_crc == crc) {
                printf("[BOOT] Firmware verified OK.\r\n");
                /* 保持 COMPLETE 状态，由 App 启动成功后写参数区确认
                 *（COMPLETE -> IDLE + boot_count=0，见 app/task_entry.c
                 * App_Ota_Confirm_Active）。 */
                return 1;
            }

            // CRC 失败且未超限：重启重试（boot_count 已持久化，重启后递增；达上限则回滚）
            printf("[BOOT] CRC mismatch (attempt %u/%u), reboot to retry...\r\n",
                   g_ota_param.boot_count, g_ota_param.max_boot_count);
            NVIC_SystemReset();
            return 0;   /* 不可达，保险 */
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
