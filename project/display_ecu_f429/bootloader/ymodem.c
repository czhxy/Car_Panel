/**
  ******************************************************************************
  * @file    ymodem.c
  * @brief   YMODEM-1K 协议接收实现
  *
  *          协议流程:
  *            MCU 持续发 'C' (轮询) -> PC 发文件名包(seq=0) -> MCU ACK
  *            -> MCU 擦 Flash -> 发 'C' 请求数据
  *            -> 数据包 seq=1..N (STX 1024B) -> MCU 每包 ACK
  *            -> PC 发 EOT -> MCU NAK -> PC 第二次 EOT -> MCU ACK
  *            -> PC 发空文件名包 -> MCU ACK -> 完成
  *
  *          USART1(PA9/PA10): YMODEM + printf 共用
  *          CRC16: 多项式 0x1021, 初始值 0x0000
  ******************************************************************************
  */

#include "ymodem.h"
#include "boot_config.h"
#include "flash_control.h"
#include "ota_params.h"
#include "stm32f4xx_usart.h"
#include <string.h>
#include <stdio.h>

// ===== 底层串口收发 (USART1 — YMODEM 协议) =====

static int uart_putc(uint8_t c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, c);
    return 0;
}

static int uart_getc_timeout(uint8_t *c, uint32_t timeout_ms)
{
    uint32_t cnt = timeout_ms * 10000;
    while (cnt--) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) {
            *c = USART_ReceiveData(USART1);
            return 1;
        }
    }
    return 0;
}

static void uart_flush_rx(void)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) {
        USART_ReceiveData(USART1);
    }
}

// ===== CRC16 (多项式 0x1021, 初始值 0x0000) =====

static uint16_t crc16_calc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// ===== 接收一包数据 =====
// 从已知包头继续接收包的剩余部分（序号、数据、CRC）
// 注意：此段不能有 printf，CPU 忙时 USART1 RX 会溢出

static int ymodem_recv_packet_body(uint8_t hdr, uint8_t *buf, uint8_t *pkt_seq)
{
    uint16_t data_len = (hdr == STX) ? PKT_DATA_1K : 128;

    uint8_t seq, seq_inv;
    if (!uart_getc_timeout(&seq, YMODEM_TIMEOUT_MS))     return YMODEM_ERR_TIMEOUT;
    if (!uart_getc_timeout(&seq_inv, YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;

    if ((uint8_t)(seq + seq_inv) != 0xFF) {
        return YMODEM_ERR_SEQ;
    }

    *pkt_seq = seq;

    for (uint16_t i = 0; i < data_len; i++) {
        if (!uart_getc_timeout(&buf[i], YMODEM_TIMEOUT_MS))
            return YMODEM_ERR_TIMEOUT;
    }

    uint8_t crc_buf[2];
    if (!uart_getc_timeout(&crc_buf[0], YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;
    if (!uart_getc_timeout(&crc_buf[1], YMODEM_TIMEOUT_MS)) return YMODEM_ERR_TIMEOUT;

    uint16_t crc_rx = ((uint16_t)crc_buf[0] << 8) | crc_buf[1];
    uint16_t crc_cal = crc16_calc(buf, data_len);

    if (crc_rx != crc_cal) {
        return YMODEM_ERR_CRC;
    }

    return data_len;
}

static int ymodem_recv_packet(uint8_t *buf, uint8_t *pkt_seq)
{
    uint8_t hdr;
    if (!uart_getc_timeout(&hdr, YMODEM_TIMEOUT_MS)) {
        return YMODEM_ERR_TIMEOUT;
    }

    if (hdr == EOT) return 0;
    if (hdr == CAN) return YMODEM_ERR_CANCEL;

    if (hdr != SOH && hdr != STX) {
        return -1;
    }

    return ymodem_recv_packet_body(hdr, buf, pkt_seq);
}

// ===== 解析文件名包 (序号 0, 128 字节) =====

static int parse_filename_packet(uint8_t *buf, ymodem_status_t *status)
{
    int name_len = 0;
    while (name_len < 64 && buf[name_len] != 0x00) name_len++;
    if (name_len >= 64 || name_len == 0) return -1;

    memcpy(status->file_name, buf, name_len);
    status->file_name[name_len] = '\0';

    int pos = name_len + 1;

    char size_str[16] = {0};
    int s = 0;
    while (pos < 128 && buf[pos] != 0x00 && buf[pos] != 0x20 && s < 15) {
        size_str[s++] = buf[pos++];
    }
    size_str[s] = '\0';

    status->file_size = 0;
    for (int i = 0; i < s; i++) {
        if (size_str[i] >= '0' && size_str[i] <= '9')
            status->file_size = status->file_size * 10 + (size_str[i] - '0');
    }
    return 0;
}

// ===== YMODEM 接收主函数 =====

int ymodem_receive(uint32_t target_addr, uint32_t max_size,
                   ymodem_status_t *status)
{
    static uint8_t rx_buf[1024];
    int retries = 0;
    uint8_t expected_seq = 0;

    memset(status, 0, sizeof(ymodem_status_t));

    printf("[YMODEM] Starting, target=0x%08X max=%u\r\n",
           (unsigned int)target_addr, (unsigned int)max_size);

    uart_flush_rx();

    // ===== Step 1: 持续发 'C' 轮询文件名包 =====
    printf("[YMODEM] Polling for filename...\r\n");

    int got_filename = 0;
    for (int i = 0; i < 300; i++) {
        uart_putc(C_CHAR);

        uint8_t hdr;
        if (uart_getc_timeout(&hdr, 100)) {
            if (hdr == SOH || hdr == STX) {
                uint8_t seq;
                int len = ymodem_recv_packet_body(hdr, rx_buf, &seq);
                if (len > 0 && seq == 0) {
                    if (parse_filename_packet(rx_buf, status) == 0) {
                        if (status->file_size > max_size) {
                            printf("[YMODEM] ERROR: File too large!\r\n");
                            return YMODEM_ERR_FILESIZE;
                        }
                        uart_putc(ACK);
                        printf("[YMODEM] File: %s, Size: %u\r\n",
                               status->file_name,
                               (unsigned int)status->file_size);
                        got_filename = 1;
                        break;
                    }
                }
            }
        }
    }

    if (!got_filename) {
        printf("[YMODEM] Timeout waiting for filename.\r\n");
        return YMODEM_ERR_TIMEOUT;
    }

    // ===== Step 2: 擦除目标分区 =====
    flash_if_init();
    printf("[YMODEM] Erasing...\r\n");
    if (flash_if_erase(target_addr, max_size) != 0) {
        printf("[YMODEM] Erase FAILED!\r\n");
        flash_if_lock();
        return YMODEM_ERR_FLASH;
    }
    uart_putc(C_CHAR);
    printf("[YMODEM] Erase OK, requesting data...\r\n");

    expected_seq = 1;

    // ===== Step 3: 接收数据包 =====
    status->total_received = 0;
    status->packet_count = 0;
    status->error_code = 0;

    int len;
    uint8_t seq;
    while (1) {
        len = ymodem_recv_packet(rx_buf, &seq);

        if (len == 0) {
            printf("[YMODEM] EOT, total=%u\r\n",
                   (unsigned int)status->total_received);
            uart_putc(NAK);

            uint8_t eot_seq;
            len = ymodem_recv_packet(rx_buf, &eot_seq);
            if (len == 0) {
                uart_putc(ACK);
                // 会话结束：收空文件名包并应答
                len = ymodem_recv_packet(rx_buf, &eot_seq);
                if (len > 0) {
                    uart_putc(ACK);
                }
                printf("[YMODEM] Transfer complete.\r\n");
                break;
            }
            continue;
        }

        if (len < 0) {
            if (retries++ < YMODEM_MAX_RETRIES) {
                uart_putc(NAK);
                printf("[YMODEM] Pkt err %d, retry %d/%d\r\n",
                       len, retries, YMODEM_MAX_RETRIES);
                continue;
            } else {
                printf("[YMODEM] Max retries, abort.\r\n");
                flash_if_lock();
                return len;
            }
        }

        if (seq != expected_seq) {
            printf("[YMODEM] Seq err: exp %d got %d\r\n",
                   expected_seq, seq);
            if (seq == (expected_seq - 1)) {
                uart_putc(ACK);
                continue;
            }
            flash_if_lock();
            return YMODEM_ERR_SEQ;
        }

        if (flash_if_write(target_addr + status->total_received,
                           rx_buf, len) != 0) {
            printf("[YMODEM] Flash write error @ %u\r\n",
                   (unsigned int)status->total_received);
            flash_if_lock();
            return YMODEM_ERR_FLASH;
        }

        status->total_received += len;
        status->packet_count++;
        expected_seq = (expected_seq + 1) & 0xFF;
        retries = 0;

        uart_putc(ACK);

        if (status->packet_count % 16 == 0) {
            printf("[YMODEM] %u/%u (%u%%)\r\n",
                   (unsigned int)status->total_received,
                   (unsigned int)status->file_size,
                   (unsigned int)(status->total_received * 100 /
                                  status->file_size));
        }
    }

    flash_if_lock();
    printf("[YMODEM] Done: %u bytes, %u pkts.\r\n",
           (unsigned int)status->total_received,
           (unsigned int)status->packet_count);

    return YMODEM_OK;
}
