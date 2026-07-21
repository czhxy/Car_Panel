#include "drv_usart.h"

/* RX 回调（指向模块层 Usart_Rx_Event） */
usart_rx_callback usart_rx_cb = NULL;

/* ISR 累积缓冲区 */
static uint8_t  rx_buf[UART_RX_FRAME_SIZE];
static uint16_t rx_idx = 0;

void usart_rx_cb_register(usart_rx_callback cb)
{
	usart_rx_cb = cb;
}

/* USART1 初始化：PA9=TX(复用推挽), PA10=RX(上拉输入), 115200 8N1
 * 使能 RXNE 和 IDLE 中断以支持不定长帧接收 */
void drv_usart_init(void)
{
	/* 时钟：USART1 与 GPIOA 均挂 APB2 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	/* GPIO：PA9=TX 复用推挽，PA10=RX 上拉输入 */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* USART：115200, 8N1, 无流控, 收发都使能 */
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);

	/* NVIC：USART1 中断通道 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

	/* USART 使能（TX 先可用，RX 中断由 drv_usart_start_rx() 延迟打开） */
	USART_Cmd(USART1, ENABLE);
}

/* drv_usart_start_rx — 在回调注册和队列就绪后调用，使能 RX 中断
 * 先清除可能悬起的状态标志，避免使能瞬间触发中断 */
void drv_usart_start_rx(void)
{
	/* 清除可能已置位的 RXNE/IDLE/ORE 等标志 */
	(void)USART1->SR;
	(void)USART1->DR;

	rx_idx = 0;   /* 重置缓冲区索引 */

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
}

/* USART1 中断：RXNE（逐字节接收）+ IDLE（帧结束检测） */
void USART1_IRQHandler(void)
{
	/* RXNE：收到一个字节，先处理确保读完 DR */
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
		rx_buf[rx_idx++] = byte;

		/* 满 32 字节立即推帧，不需等 IDLE */
		if (rx_idx >= UART_RX_FRAME_SIZE)
		{
			if (usart_rx_cb) usart_rx_cb(rx_buf, UART_RX_FRAME_SIZE);
			rx_idx = 0;
		}
	}

	/* IDLE：线路空闲（读 SR 再读 DR 清标志），推不完整帧 */
	if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
	{
		(void)USART1->SR;   /* 清 IDLE 标志 */
		(void)USART1->DR;   /* 读 DR（读出的字节可忽略，已由 RXNE 处理） */

		if (rx_idx > 0)
		{
			if (usart_rx_cb) usart_rx_cb(rx_buf, rx_idx);
			rx_idx = 0;
		}
	}
}



/* ===== TX：轮询发送（链路验证打印用）===== */
void Usart_SendByte(uint8_t b)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, b);
}

void Usart_SendData(const uint8_t *p, uint16_t n)
{
	while (n--) Usart_SendByte(*p++);
}

void Usart_SendString(const char *s)
{
	while (*s) Usart_SendByte((uint8_t)*s++);
}
