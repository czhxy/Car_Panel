#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

#include <stdint.h>

// ======================== Flash 布局 ========================
#define FLASH_BASE_ADDR         0x08000000U
#define FLASH_TOTAL_SIZE        0x00080000U  // 512KB

// Bootloader: 48KB (Sector 0-2, 各16KB)
#define BOOTLOADER_ADDR         0x08000000U
#define BOOTLOADER_SIZE         0x0000C000U  // 48KB

// OTA Parameter: 16KB (Sector 3)
#define OTA_PARAM_ADDR          0x0800C000U
#define OTA_PARAM_SIZE          0x00004000U  // 16KB

// App A: 192KB (Sector 4:64KB + Sector 5:128KB)
#define APP_A_ADDR              0x08010000U
#define APP_A_SIZE              0x00030000U  // 192KB

// App B: 256KB (Sector 6:128KB + Sector 7:128KB)
#define APP_B_ADDR              0x08040000U
#define APP_B_SIZE              0x00040000U  // 256KB

// YMODEM 参数
#define YMODEM_PACKET_SIZE      1024        // YMODEM-1K
#define YMODEM_TIMEOUT_MS       3000        // 接收超时 (ms)
#define YMODEM_MAX_RETRIES      10          // 最大重试次数

// 回滚参数
#define MAX_BOOT_ATTEMPTS        3          // 最大启动尝试次数

// ======================== 状态常量 ========================
#define APP_A_ACTIVE            0
#define APP_B_ACTIVE            1

#define OTA_STATE_IDLE          0
#define OTA_STATE_RECEIVING     1
#define OTA_STATE_VERIFY        2
#define OTA_STATE_COMPLETE      3
#define OTA_STATE_FAILED        4

#define OTA_MAGIC               0x4F544152  // "RATO"

#endif /* __BOOT_CONFIG_H */
