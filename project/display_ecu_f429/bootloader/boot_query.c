/**
  ******************************************************************************
  * @file    boot_query.c
  * @brief   Bootloader 芯片信息查询实现 — 轮询响应 PC 上位机查询指令
  *
  * Bootloader 为裸机轮询（不能开 RXNE 中断，与 YMODEM 轮询收发冲突），
  * 因此本模块在按键等待窗口内每 1ms 轮询一次 UART_ReceiveByte()，
  * 匹配到 [AA 55 01 00] 查询指令后组包应答，格式与 app 侧 mod_query.c 一致：
  *   data[0]      MCU 型号标记 0xF4 (STM32F429)
  *   data[1]      活跃分区 1=App A / 2=App B
  *   data[2..5]   bootloader 基址 0x08000000 (LSB first)
  *   data[6..9]   活跃槽镜像基址 (LSB first)
  *   data[10..11] 软件版本号 主.次
  *   data[12]     保留 0
  * 应答帧: [AA][55][type=0x01][len=0x0D][data13][crc16_hi][crc16_lo]
  ******************************************************************************
  */

#include "boot_query.h"
#include "boot_config.h"
#include "ota_params.h"
#include "usart.h"
#include "key.h"
#include "Delay.h"
#include <string.h>

/* g_ota_param 在 boot_main.c 中定义，此处引用活跃分区 */
extern ota_param_t g_ota_param;

#define QUERY_TYPE      0x01
#define PKT_DATA_LEN    13
#define RX_IDLE         0   /* 等待 0xAA */
#define RX_GOT_AA       1   /* 等待 0x55 */
#define RX_GOT_55       2   /* 等待 0x01 */
#define RX_GOT_TYPE     3   /* 等待 0x00 */

static uint8_t s_rx_state = RX_IDLE;

/* ============================================================
 * crc16_calc — CRC16 (poly 0x1021, init 0x0000, MSB first)
 * 与 ymodem.c / mod_comm_uart.c / ota_core.py 保持一致
 * ============================================================ */
static uint16_t crc16_calc(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0;
    uint8_t  i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ============================================================
 * Boot_Query_Respond — 组包并发送芯片信息应答
 * ============================================================ */
static void Boot_Query_Respond(void)
{
    uint8_t  data[PKT_DATA_LEN];
    uint8_t  pkt[4 + PKT_DATA_LEN + 2];
    uint16_t crc;
    uint32_t app_addr;
    uint8_t  partition;
    uint8_t  i;

    if (g_ota_param.active_partition == APP_A_ACTIVE) {
        partition = 1;   /* GUI: 1 = App A */
        app_addr  = APP_A_ADDR;
    } else {
        partition = 2;   /* GUI: 2 = App B */
        app_addr  = APP_B_ADDR;
    }

    data[0] = 0xF4;                              /* STM32F429 */
    data[1] = partition;
    for (i = 0; i < 4; i++) {
        data[2 + i] = (uint8_t)(FLASH_BASE_ADDR >> (8 * i));  /* boot 基址 LE */
        data[6 + i] = (uint8_t)(app_addr >> (8 * i));         /* 活跃槽基址 LE */
    }
    data[10] = (uint8_t)(int)APP_SOFTWARE_VERSION;            /* 版本主 */
    data[11] = (uint8_t)(int)((APP_SOFTWARE_VERSION - (int)APP_SOFTWARE_VERSION)
                              * 10 + 0.5);                    /* 版本次 */
    data[12] = 0;                                             /* 保留 */

    pkt[0] = 0xAA;
    pkt[1] = 0x55;
    pkt[2] = QUERY_TYPE;
    pkt[3] = PKT_DATA_LEN;
    memcpy(&pkt[4], data, PKT_DATA_LEN);

    crc = crc16_calc(data, PKT_DATA_LEN);
    pkt[4 + PKT_DATA_LEN]     = (uint8_t)(crc >> 8);
    pkt[4 + PKT_DATA_LEN + 1] = (uint8_t)(crc & 0xFF);

    UART_SendArray(pkt, sizeof(pkt));
}

/* ============================================================
 * Boot_Query_Poll — 非阻塞轮询接收，匹配 [AA 55 01 00] 查询指令
 * 任意字节不匹配即回退到状态 0；AA 开头允许重新同步
 * ============================================================ */
void Boot_Query_Poll(void)
{
    int ch = UART_ReceiveByte();
    if (ch < 0) {
        return;
    }

    switch (s_rx_state) {
    case RX_IDLE:
        s_rx_state = (ch == 0xAA) ? RX_GOT_AA : RX_IDLE;
        break;
    case RX_GOT_AA:
        s_rx_state = (ch == 0x55) ? RX_GOT_55 : ((ch == 0xAA) ? RX_GOT_AA : RX_IDLE);
        break;
    case RX_GOT_55:
        s_rx_state = (ch == 0x01) ? RX_GOT_TYPE : ((ch == 0xAA) ? RX_GOT_AA : RX_IDLE);
        break;
    case RX_GOT_TYPE:
        if (ch == 0x00) {
            Boot_Query_Respond();
        }
        s_rx_state = (ch == 0xAA) ? RX_GOT_AA : RX_IDLE;
        break;
    default:
        s_rx_state = RX_IDLE;
        break;
    }
}

/* ============================================================
 * Boot_Query_WaitPress — 按键等待窗口（与 key_wait_press 等价），
 * 期间每毫秒轮询一次查询指令，兼顾"2s 内查询芯片信息"与"按键进 OTA"
 * ============================================================ */
int Boot_Query_WaitPress(uint32_t timeout_ms)
{
    while (timeout_ms--) {
        Boot_Query_Poll();
        if (key_is_pressed()) {
            return 1;
        }
        Delay_ms(1);
    }
    return 0;
}
