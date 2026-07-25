#ifndef __MOD_USART_H
#define __MOD_USART_H

#include "stm32f10x.h"                  // Device header
#include <stdint.h>

/* RX 帧结构：最多 31 字节数据 + 长度（对齐 32 字节，配合队列 item_size） */
typedef struct {
	uint8_t data[31];
	uint8_t len;
} UsartRxFrame;

/* ---- 初始化 ---- */
void Mod_Usart_Init(void);

/* ---- RX 路径 ---- */
/* ISR 回调：收到一帧数据推入 RX 队列 */
void Usart_Rx_Event(const uint8_t *buf, uint16_t len);
/* 主循环调用：处理 RX 队列数据，累积行缓冲并解析命令 */
void Usart_Rx_Process(void);

/* ---- TX 路径 ---- */
/* 业务层推送待发送数据到 TX 队列 */
void Usart_Tx_Event(const uint8_t *data, uint16_t len);
/* 主循环调用：从 TX 队列出队并发送 */
void Usart_Tx_Process(void);

/* ---- 命令解析（弱符号，业务层可覆盖）---- */
void Usart_ParseCommand(const char *cmd);

/* ---- 错误日志：通过 TX 队列统一推送，20ms 周期消费 ---- */
void Uart_Error(const char *msg);

#endif
