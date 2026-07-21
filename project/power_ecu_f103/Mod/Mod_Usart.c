#include "Mod_Usart.h"
#include "drv_usart.h"
#include "queue.h"
#include <string.h>

/* ===== 队列与缓冲池 ===== */

/* RX 队列：UsartRxFrame pool[4]（4×32=128 字节），item_size=32，3 帧可用 */
#define UART_RX_QUEUE_SIZE  4
#define UART_RX_ITEM_SIZE   sizeof(UsartRxFrame)

static QueueType  UsartRxQueue;
static UsartRxFrame rx_pool[UART_RX_QUEUE_SIZE];

/* TX 队列：uint8_t pool[512]，item_size=1，511 字节可用 */
#define UART_TX_QUEUE_SIZE  512

static QueueType UsartTxQueue;
static uint8_t   tx_pool[UART_TX_QUEUE_SIZE];

/* 错误计数器 */
static uint32_t uart_tx_err_count = 0;
static uint32_t uart_rx_err_count = 0;

/* ===== 行缓冲（主循环上下文）===== */
static char     line_buf[64];
static uint8_t  line_idx = 0;

/* ===== 前向声明 ===== */
static void Usart_SendLine(const char *s);

/* ---- 初始化 ---- */
void Mod_Usart_Init(void)
{
	drv_usart_init();
	usart_rx_cb_register(Usart_Rx_Event);

	Queue_Init(&UsartRxQueue, rx_pool, sizeof(rx_pool), UART_RX_ITEM_SIZE);
	Queue_Init(&UsartTxQueue, tx_pool, sizeof(tx_pool), 1);

	line_idx = 0;
	uart_tx_err_count = 0;
	uart_rx_err_count = 0;

	/* 最后使能 RX 中断（此时回调、队列、缓冲已全部就绪） */
	drv_usart_start_rx();
}

/* ===== RX 路径 ===== */

/* Usart_Rx_Event(buf, len) — ISR 上下文调用
 * 构造 UsartRxFrame 并推入 RX 队列（截断到 31 字节） */
void Usart_Rx_Event(const uint8_t *buf, uint16_t len)
{
	UsartRxFrame frame;
	uint16_t copy_len = (len > 31) ? 31 : len;

	memcpy(frame.data, buf, copy_len);
	frame.len = (uint8_t)copy_len;

	if (!Queue_Put(&UsartRxQueue, &frame))
	{
		uart_rx_err_count++;  /* 队列满，丢弃 */
	}
}

/* Usart_Rx_Process() — 主循环 20ms 调用
 * 遍历 RX 队列所有帧，逐字节累积到行缓冲，遇 \r/\n 解析命令 */
void Usart_Rx_Process(void)
{
	UsartRxFrame frame;

	/* 处理 RX 队列中所有帧 */
	while (Queue_Get(&UsartRxQueue, &frame))
	{
		uint8_t i;
		for (i = 0; i < frame.len; i++)
		{
			char ch = (char)frame.data[i];

			/* 换行或回车：行结束，解析命令 */
			if (ch == '\r' || ch == '\n')
			{
				if (line_idx > 0)
				{
					line_buf[line_idx] = '\0';
					Usart_SendString("\r\n");
					Usart_ParseCommand(line_buf);
					Usart_SendString("\r\n> ");
					line_idx = 0;
				}
				/* 跳过连续的 \r\n */
				continue;
			}

			/* 普通字符：回显 + 存入行缓冲 */
			Usart_SendByte((uint8_t)ch);

			/* 行缓冲溢出（>63 字节）：静默丢弃已存部分 */
			if (line_idx >= sizeof(line_buf) - 1)
			{
				line_idx = 0;
				continue;
			}
			line_buf[line_idx++] = ch;
		}
	}
}

/* ===== TX 路径 ===== */

/* Usart_Tx_Event(data, len) — 业务层推送待发送数据 */
void Usart_Tx_Event(const uint8_t *data, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++)
	{
		if (!Queue_Put(&UsartTxQueue, (void *)&data[i]))
		{
			uart_tx_err_count++;
			/* 继续尝试后续字节，不跳出循环 */
		}
	}
}

/* Usart_Tx_Process() — 主循环 20ms 调用
 * Query-Get 模式，每次最多发 16 字节（约 1.4ms） */
void Usart_Tx_Process(void)
{
	uint8_t byte;
	uint8_t cnt = 0;

	/* Query-Get 模式（与 CAN 发送一致） */
	while (cnt < 16 && Queue_Query(&UsartTxQueue, &byte))
	{
		(void)Queue_Get(&UsartTxQueue, &byte);
		Usart_SendByte(byte);
		cnt++;
	}
}

/* ===== 命令解析（弱符号，业务层可覆盖）===== */
__weak void Usart_ParseCommand(const char *cmd)
{
	if (cmd == NULL) return;

	if (strncmp(cmd, "echo ", 5) == 0)
	{
		Usart_SendString(cmd + 5);
	}
	else if (strcmp(cmd, "help") == 0)
	{
		Usart_SendLine("=== UART Console Help ===");
		Usart_SendLine("echo <msg>  - echo message");
		Usart_SendLine("help       - show help");
		Usart_SendLine("status     - query status (extendable)");
	}
	else
	{
		/* 未知命令提示 */
		Usart_SendString("? ");
		Usart_SendString(cmd);
	}
}

/* ---- 内部辅助 ---- */
static void Usart_SendLine(const char *s)
{
	Usart_SendString(s);
	Usart_SendString("\r\n");
}
