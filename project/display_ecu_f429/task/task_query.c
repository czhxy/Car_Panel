/**
  ******************************************************************************
  * @file    task_query.c
  * @brief   UART Query Task — 0xAA 0x55 protocol, interrupt-driven RX
  *
  * Command: 0x01 = chip info query
  * Depends: USART1 (RXNE interrupt -> ring buffer -> UART_RxGet)
  *
  * M3/M4: 地址/分区不再本地重定义、也不再读 OTA 参数区；运行槽位由 SCB->VTOR
  *        自证（App A 链接于 0x08020000、App B 链接于 0x08080000，VTOR 即运行基址）。
  ******************************************************************************
  */
#include "task_query.h"
#include "main.h"
#include "boot_config.h"   /* M3: 地址常量单一来源（FLASH_BASE_ADDR / APP_A_ADDR / APP_B_ADDR） */
#include "usart.h"
#include "bsp_log.h"
#include "task_comm_can_protocol.h"   /* 链路测试：UART 字节透传到 CAN(0x080) */
#include "CAN_Protocol.h"             /* CAN_PRIO_* / CAN_ADDR_* / MODE_ID_* 常量 */
#include <string.h>

// ======================== Protocol constants ========================
#define PKT_HEADER1             0xAA
#define PKT_HEADER2             0x55
#define CMD_CHIP_INFO           0x01
#define PKT_DATA_LEN            13

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
    uint32_t boot_addr  = FLASH_BASE_ADDR;
    uint32_t active_app = SCB->VTOR;                       /* 当前运行镜像基址 */
    uint8_t  partition  = (active_app == APP_A_ADDR) ? 1 : 2;  /* M4: A=1 / B=2，自证 */

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

    /* 链路测试脚手架：把串口收到的每个字节透传到动力域(CAN, mode 0x080)。
     * 累计进 fwd，满 8 字节或本轮 burst 结束就发一帧；chip-info 命令解析照常保留。 */
    uint8_t fwd[8];
    uint8_t fwd_len = 0;

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

            /* 透传：累计字节，满 8 就发一帧 CAN 到动力域 */
            fwd[fwd_len++] = (uint8_t)ch;
            if (fwd_len >= 8) {
                CanProto_SendFrame(CAN_PRIO_QUERY_REPLY, CAN_ADDR_MOTORBOARD,
                                   CAN_FTYPE_NORMAL, MODE_ID_QUERY_FAST, 0,
                                   fwd, fwd_len);
                fwd_len = 0;
            }
        }

        /* burst 结束：不足 8 字节的剩余也发出去 */
        if (fwd_len > 0) {
            CanProto_SendFrame(CAN_PRIO_QUERY_REPLY, CAN_ADDR_MOTORBOARD,
                               CAN_FTYPE_NORMAL, MODE_ID_QUERY_FAST, 0,
                               fwd, fwd_len);
            fwd_len = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
