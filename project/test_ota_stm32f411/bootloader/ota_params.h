#ifndef __OTA_PARAMS_H
#define __OTA_PARAMS_H

#include <stdint.h>
#include "boot_config.h"

// ===== OTA 参数结构体（存储在 0x0800 C000，Sector 3） =====
typedef struct __attribute__((packed)) {
    uint32_t magic;                 // 魔数 0x4F544152 ("RATO")
    uint32_t param_version;         // 参数版本号

    uint8_t  active_partition;      // 当前活跃分区: 0=App A, 1=App B
    uint8_t  ota_state;             // OTA 状态: 0=IDLE, 1=RECEIVING,
                                    //           2=VERIFY, 3=COMPLETE, 4=FAILED
    uint8_t  boot_count;            // 启动尝试计数
    uint8_t  max_boot_count;        // 最大启动尝试次数

    // App A 固件信息
    uint32_t app_a_version;         // 版本号 (0x00010000 = v1.0.0)
    uint32_t app_a_size;            // 固件大小 (bytes)
    uint32_t app_a_crc32;           // CRC32 校验值

    // App B 固件信息
    uint32_t app_b_version;
    uint32_t app_b_size;
    uint32_t app_b_crc32;

    // 保留（可用于扩展）
    uint32_t reserved[4];
} ota_param_t;

// ===== CRC32 函数（阶段四） =====

// 标准 CRC-32/MPEG-2（多项式 0xEDB88320，反射）
uint32_t crc32_calc(const uint8_t *data, uint32_t len);

// 计算 Flash 中指定地址范围的 CRC32
uint32_t crc32_flash(uint32_t addr, uint32_t len);

// ===== 参数管理函数（阶段五） =====
int  ota_params_init(void);
int  ota_params_load(ota_param_t *param);
int  ota_params_save(const ota_param_t *param);

#endif /* __OTA_PARAMS_H */
