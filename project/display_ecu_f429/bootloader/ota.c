/**
  ******************************************************************************
  * @file    ota.c
  * @brief   OTA 升级 — 真 AB：YMODEM 写入【非活跃槽】-> CRC32 -> 翻转 active -> 重启
  *
  *          方案：A、B 两槽等大镜像。OTA 把新固件写入当前非活跃槽（不影响正在
  *          运行的活跃槽），校验通过后翻转 active_partition 并重启；bootloader
  *          下次启动跳到新槽。原活跃槽固件完整保留，作为回滚目标（见 boot_decision）。
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
#include <ctype.h>

// g_ota_param 在 boot_main.c 中定义
extern ota_param_t g_ota_param;

// ===== 从 YMODEM 文件名解析版本号 (M2) =====
// 约定文件名形如 "xxx_v1_2.bin" / "xxx_v1.2.bin"（v 不区分大小写）。
// 解析成功返回 (major<<16)|minor；失败返回 0（未知）。
static uint32_t parse_version_from_name(const char *name)
{
    if (name == NULL) {
        return 0;
    }

    for (const char *p = name; *p != '\0'; p++) {
        if (tolower((unsigned char)*p) != 'v') {
            continue;
        }
        const char *s = p + 1;
        if (!isdigit((unsigned char)*s)) {
            continue;
        }

        uint32_t maj = 0;
        while (isdigit((unsigned char)*s)) {
            maj = maj * 10 + (uint32_t)(*s - '0');
            s++;
        }
        if (*s != '.' && *s != '_') {
            continue;
        }
        s++;

        uint32_t min = 0;
        if (!isdigit((unsigned char)*s)) {
            continue;
        }
        while (isdigit((unsigned char)*s)) {
            min = min * 10 + (uint32_t)(*s - '0');
            s++;
        }
        return (maj << 16) | (min & 0xFFFF);
    }
    return 0;
}

// ===== YMODEM 写入非活跃槽 + 校验 + 翻转 active + 重启 =====
void ota_ymodem_start(void)
{
    // 目标 = 当前非活跃槽（写入它不影响正在运行的活跃槽）
    uint8_t  target_part = (g_ota_param.active_partition == APP_A_ACTIVE)
                           ? APP_B_ACTIVE : APP_A_ACTIVE;
    uint32_t target_addr = (target_part == APP_A_ACTIVE)
                           ? APP_A_ADDR : APP_B_ADDR;
    uint32_t target_size = (target_part == APP_A_ACTIVE)
                           ? APP_A_SIZE : APP_B_SIZE;

    printf("[BOOT] Starting YMODEM OTA...\r\n");
    printf("[BOOT] Active=%s, target(inactive)=%s @ 0x%08X (%uKB)\r\n",
           g_ota_param.active_partition == APP_A_ACTIVE ? "App A" : "App B",
           target_part == APP_A_ACTIVE ? "App A" : "App B",
           (unsigned int)target_addr,
           (unsigned int)(target_size / 1024));

    ymodem_status_t status;
    int ret = ymodem_receive(target_addr, target_size, &status);

    if (ret == YMODEM_OK) {
        printf("[BOOT] YMODEM transfer OK, verifying...\r\n");

        // 校验镜像链接地址是否匹配目标槽（防错包）：
        // 位置链接的镜像，其向量表 reset-handler(偏移 +4) 必然落在自身槽地址范围内。
        // 若落在另一槽范围，说明上位机发错了 bin（如活跃 A 时发了 app.bin 而非 app_b.bin）。
        uint32_t reset_handler = *(volatile uint32_t *)(target_addr + 4);
        if (reset_handler < target_addr ||
            reset_handler >= target_addr + target_size) {
            printf("[BOOT] ERROR: image reset-handler 0x%08X not in target slot "
                   "[0x%08X-0x%08X) - wrong bin for this slot! Aborting (active unchanged).\r\n",
                   (unsigned int)reset_handler,
                   (unsigned int)target_addr,
                   (unsigned int)(target_addr + target_size));
            return;   /* 不翻转 active、不重启；活跃槽不受影响，回到重试循环 */
        }

        uint32_t crc = crc32_flash(target_addr, status.total_received);
        uint32_t ver = parse_version_from_name((const char *)status.file_name);
        printf("[BOOT] CRC32: 0x%08X, version: 0x%08X (from '%s')\r\n",
               (unsigned int)crc, (unsigned int)ver,
               (const char *)status.file_name);

        // 记录新槽元数据（旧槽元数据保持不变，作为回滚依据）
        if (target_part == APP_A_ACTIVE) {
            g_ota_param.app_a_version = ver;
            g_ota_param.app_a_size    = status.total_received;
            g_ota_param.app_a_crc32   = crc;
        } else {
            g_ota_param.app_b_version = ver;
            g_ota_param.app_b_size    = status.total_received;
            g_ota_param.app_b_crc32   = crc;
        }

        // 翻转 active 到新槽，标记 COMPLETE 待下次启动校验
        g_ota_param.active_partition = target_part;
        g_ota_param.ota_state        = OTA_STATE_COMPLETE;
        g_ota_param.boot_count       = 0;
        ota_params_save(&g_ota_param);

        printf("[BOOT] OTA params updated, active=%s. Rebooting...\r\n",
               target_part == APP_A_ACTIVE ? "App A" : "App B");
        Delay_ms(500);
        NVIC_SystemReset();
    } else {
        printf("[BOOT] YMODEM failed (code: %d). Active slot untouched.\r\n", ret);
    }
}
