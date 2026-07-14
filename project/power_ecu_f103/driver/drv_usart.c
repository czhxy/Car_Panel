#include "drv_usart.h"

usart_rx_callback usart_rx_cb = NULL;

void usart_rx_cb_register(usart_rx_callback cb)
{
	usart_rx_cb = cb;
}

/* USART1 初始化：PA9=TX(复用推挽), PA10=RX(上拉输入), 115200 8N1
 * 说明：链路验证阶段只用到 TX（把收到的 CAN 数据打印出来），故暂不使能 RX 中断。
 *       需要串口接收时，在此处加 USART_ITConfig(USART1, USART_IT_RXNE, ENABLE)，
 *       并在 USART1_IRQHandler 里把 DR 读入缓冲即可。 */
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

	/* USART：115200, 8N1, 无流控, 收发都使能
	 * （原代码此处写成 Tx|Tx 漏了 Rx，已修正为 Tx|Rx） */
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);

	/* NVIC：USART1 中断通道（当前未使能 RX 中断源，故不会触发，仅预留） */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1, ENABLE);
}

/* USART1 中断：当前未使能任何中断源，本函数实际不会进入，预留避免向量默认挂死 */
void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
	{
		(void)USART1->SR;            /* 读 SR 再读 DR 清 IDLE */
		(void)USART1->DR;
	}
}

/* ===== TX：轮询发送（链路验证打印用，仿显示域 fputc 轮询 TXE）===== */
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
