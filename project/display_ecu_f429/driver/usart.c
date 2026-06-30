#include "usart.h"

// ===== USART1 (PA9/PA10) — 串口通信 =====
void UART_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	// STM32F42x/43x: GPIO AF 配置需要使能 SYSCFG 时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;

	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);

	USART_InitStructure.USART_BaudRate            = 115200;
	USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits            = USART_StopBits_1;
	USART_InitStructure.USART_Parity              = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);

	// 中断和 NVIC 在 USART 初始化之后配置（Best Practice）
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 6;
	NVIC_Init(&NVIC_InitStructure);
}

// ===== printf 重定向到 USART1 =====
int fputc(int ch, FILE *f)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, (uint8_t)ch);
	return ch;
}

// ===== RX 环形缓冲区 (ISR → 任务) =====
#define RX_BUF_SIZE  64
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

// ===== 非阻塞接收 (无数据时返回 -1) =====
int UART_ReceiveByte(void)
{
    // 检查并清除溢出标志 (ORE 不清理会阻塞后续接收)
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        (void)USART1->SR;   // 读 SR 清 ORE
        (void)USART1->DR;   // 读 DR 完成清除序列
    }

    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
        return (int)USART_ReceiveData(USART1);
    }
    return -1;
}

// ===== 从中断接收环形缓冲区取一字节 (任务调用) =====
int UART_RxGet(void)
{
    uint8_t byte;
    if (rx_head != rx_tail) {
        byte = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        return (int)byte;
    }
    return -1;
}

void UART_SendByte(uint8_t Byte)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, Byte);
}

void UART_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i++) {
		UART_SendByte(Array[i]);
	}
}

void UART_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++) {
		UART_SendByte(String[i]);
	}
}

static uint32_t UART_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--) {
		Result *= X;
	}
	return Result;
}

void UART_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++) {
		UART_SendByte(Number / UART_Pow(10, Length - i - 1) % 10 + '0');
	}
}

void UART_Printf(char *format, ...)
{
	char String[200];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	UART_SendString(String);
}

void USART1_IRQHandler(void)
{
	// RXNE: 收到数据，读 DR 存入环形缓冲区
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
		uint8_t ch = (uint8_t)USART_ReceiveData(USART1);   // 读 DR 自动清 RXNE
		uint8_t next = (rx_head + 1) % RX_BUF_SIZE;
		if (next != rx_tail) {   // 缓冲区未满
			rx_buf[rx_head] = ch;
			rx_head = next;
		}
		// 缓冲区满则丢弃，避免死锁
	}

	// ORE: 溢出，读 SR+DR 清除
	if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
		(void)USART1->SR;
		(void)USART1->DR;
	}
}
