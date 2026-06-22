#ifndef __YMODEM_H
#define __YMODEM_H

#include <stdint.h>

// ===== YMODEM 控制字符 =====
#define SOH         0x01   // 128 字节包头
#define STX         0x02   // 1024 字节包头 (YMODEM-1K)
#define EOT         0x04   // 传输结束
#define ACK         0x06   // 确认
#define NAK         0x15   // 未确认（重传请求）
#define CAN         0x18   // 取消传输
#define C_CHAR      0x43   // 'C'，请求 CRC16 模式

// ===== YMODEM 包结构 =====
#define PKT_HEADER  3      // STX + 序号 + 反码
#define PKT_DATA_1K 1024   // 1KB 数据
#define PKT_CRC     2      // 2 字节 CRC16

// ===== 错误码 =====
#define YMODEM_OK             0
#define YMODEM_ERR_TIMEOUT   -1
#define YMODEM_ERR_CRC       -2
#define YMODEM_ERR_SEQ       -3
#define YMODEM_ERR_FLASH     -4
#define YMODEM_ERR_CANCEL    -5
#define YMODEM_ERR_FILESIZE  -6

// ===== 状态结构体 =====
typedef struct {
    uint8_t  file_name[64];
    uint32_t file_size;
    uint32_t total_received;
    uint32_t packet_count;
    int      error_code;
} ymodem_status_t;

// ===== 接口 =====
int ymodem_receive(uint32_t target_addr, uint32_t max_size,
                   ymodem_status_t *status);

#endif /* __YMODEM_H */
