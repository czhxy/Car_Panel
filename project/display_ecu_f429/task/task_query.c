/**
  ******************************************************************************
  * @file    task_query.c
  * @brief   UART Query Task — 0xAA 0x55 protocol, interrupt-driven RX
  *
  * Command: 0x01 = chip info query
  * Depends: USART1 (RXNE interrupt → ring buffer → UART_RxGet)
  ******************************************************************************
  */
#include "task_query.h"
#include "main.h"
#include "usart.h"
#include "bsp_log.h"
#include <string.h>

// ======================== Flash address ========================
#define FLASH_BASE_ADDR         0x08000000U
#define APP_A_ADDR              0x08020000U
#define APP_B_ADDR              0x08080000U

// ======================== OTA params struct ========================
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t param_version;
    uint8_t  active_partition;
    uint8_t  ota_state;
    uint8_t  boot_count;
    uint8_t  max_boot_count;
    uint32_t app_a_version;
    uint32_t app_a_size;
    uint32_t app_a_crc32;
    uint32_t app_b_version;
    uint32_t app_b_size;
    uint32_t app_b_crc32;
    uint32_t reserved[4];
} ota_param_snap_t;

#define OTA_PARAM_ADDR          0x08010000U

// ======================== Protocol constants ========================
#define PKT_HEADER1             0xAA
#define PKT_HEADER2             0x55
#define CMD_CHIP_INFO           0x01
#define PKT_DATA_LEN            13
#define APP_A_ACTIVE            0
#define APP_B_ACTIVE            1

// ======================== CRC16 (poly 0x1021) ========================
static uint16_t crc16_calc(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// ======================== Packet send ========================
static void send_packet(uint8_t type, const uint8_t *data, uint8_t len)
{
    uint8_t buf[32];
    uint8_t idx = 0;

    buf[idx++] = PKT_HEADER1;
    buf[idx++] = PKT_HEADER2;
    buf[idx++] = type;
    buf[idx++] = len;
    memcpy(&buf[idx], data, len);
    idx += len;

    uint16_t crc = crc16_calc(data, len);
    buf[idx++] = (uint8_t)(crc >> 8);
    buf[idx++] = (uint8_t)(crc & 0xFF);

    taskENTER_CRITICAL();
    UART_SendArray(buf, idx);
    taskEXIT_CRITICAL();
}

// ======================== Command 0x01: Chip info query ========================
static void handle_chip_info_query(void)
{
    const ota_param_snap_t *ota = (const ota_param_snap_t *)OTA_PARAM_ADDR;

    uint32_t boot_addr  = FLASH_BASE_ADDR;
    uint32_t active_app = (ota->active_partition == APP_A_ACTIVE)
                          ? APP_A_ADDR : APP_B_ADDR;
    uint8_t  partition  = (ota->active_partition == APP_A_ACTIVE) ? 1 : 2;

    uint8_t data[PKT_DATA_LEN];
    data[0] = 0xF4;
    data[1] = partition;
    data[2] = (uint8_t)(boot_addr);
    data[3] = (uint8_t)(boot_addr >> 8);
    data[4] = (uint8_t)(boot_addr >> 16);
    data[5] = (uint8_t)(boot_addr >> 24);
    data[6] = (uint8_t)(active_app);
    data[7] = (uint8_t)(active_app >> 8);
    data[8] = (uint8_t)(active_app >> 16);
    data[9] = (uint8_t)(active_app >> 24);
    data[10] = (uint8_t)((int)APP_VERSION);
    data[11] = (uint8_t)((int)((APP_VERSION - (int)APP_VERSION) * 10 + 0.5));
    data[12] = 0;

    send_packet(CMD_CHIP_INFO, data, PKT_DATA_LEN);
}

// ======================== Task main loop ========================
void UART_Query_Task(void *pvParameters)
{
    (void)pvParameters;
    LOG_I("[QUERY] Task started\r\n");

    uint8_t state = 0;  // 0=wait HEADER1, 1=wait HEADER2, 2=wait TYPE

    while (1)
    {
        // Burst read from interrupt ring buffer
        int ch;
        while ((ch = UART_RxGet()) >= 0) {
            switch (state) {
            case 0:
                if (ch == PKT_HEADER1) state = 1;
                break;
            case 1:
                state = (ch == PKT_HEADER2) ? 2 : 0;
                break;
            case 2:
                if (ch == CMD_CHIP_INFO) {
                    handle_chip_info_query();
                }
                state = 0;
                break;
            default:
                state = 0;
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
