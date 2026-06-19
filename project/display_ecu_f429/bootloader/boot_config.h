#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

#include <stdint.h>

// ============ Flash 布局 (STM32F429IGTx, 1MB, 扇区对齐, 不跨扇区) ============
#define FLASH_BASE_ADDR         0x08000000U
#define FLASH_TOTAL_SIZE        0x00100000U  // 1MB

// Bootloader: 64KB (Sector 0-3, 各16KB)
#define BOOTLOADER_ADDR         0x08000000U
#define BOOTLOADER_SIZE         0x00010000U  // 64KB

// OTA Parameter: 16KB (位于 Sector 4, 64KB 扇区内)
#define OTA_PARAM_ADDR          0x08010000U
#define OTA_PARAM_SIZE          0x00004000U  // 16KB (逻辑大小, 擦除整扇区 64KB)

// App A: 384KB (Sector 5-7, 3×128KB, 避免与 App B 共享 Sector 8)
#define APP_A_ADDR              0x08020000U
#define APP_A_SIZE              0x00060000U  // 384KB

// App B: 512KB (Sector 8-11, 4×128KB)
#define APP_B_ADDR              0x08080000U
#define APP_B_SIZE              0x00080000U  // 512KB

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
