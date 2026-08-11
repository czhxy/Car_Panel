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
	// Bootloader 使用 YMODEM 轮询收发，不得开启 RX 中断（与 uart_getc_timeout 轮询冲突）
#ifndef BOOTLOADER
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0; /* Group_4 下子优先级无位，必须为 0 */
	NVIC_Init(&NVIC_InitStructure);
#endif
}

// ===== printf 重定向到 USART1 =====
int fputc(int ch, FILE *f)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, (uint8_t)ch);
	return ch;
}

// ===== 非阻塞接收 (无数据时返回 -1, Bootloader YMODEM 轮询使用) =====
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

void UART_SendByte(uint8_t Byte)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, Byte);
}

void UART_SendArray(const uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i++) {
		UART_SendByte(Array[i]);
	}
}

void UART_SendString(const char *String)
{
	size_t i;
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
	size_t i;
	for (i = 0; i < Length; i++) {
		UART_SendByte(Number / UART_Pow(10, Length - i - 1) % 10 + '0');
	}
}

void UART_Printf(char *format, ...)
{
	char String[200];
	va_list arg;
	va_start(arg, format);
	vsnprintf(String, sizeof(String), format, arg);
	va_end(arg);
	UART_SendString(String);
}

/* ===== 日志发送（弱符号默认实现）=====
 * 默认等价 printf 直接发送；App 由 mod_comm_uart.c 强符号覆盖为「入 UART 日志队列
 * → Task_UartTx 统一消费」，bootloader / 无队列场景保持本默认行为。
 * ===== */
#if defined(__GNUC__) && !defined(__CC_ARM)
  #define UART_WEAK __attribute__((weak))
#else
  #define UART_WEAK __weak
#endif

UART_WEAK void UART_Log(const char *format, ...)
{
	char buf[128];
	va_list arg;
	va_start(arg, format);
	vsnprintf(buf, sizeof(buf), format, arg);
	va_end(arg);
	UART_SendString(buf);
}

/* USART1_IRQHandler 已在 firmware/cmsis/device/stm32f4xx_it.c 中定义（App 专用），
 * 转发到 task 层 Mod_Uart_RxIRQHandler()，与 CAN1_RX0_IRQHandler 对齐。
 * 本文件为纯硬件层，不依赖任何 task 模块。 */
