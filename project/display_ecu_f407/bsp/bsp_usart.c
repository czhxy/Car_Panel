#include "bsp_usart.h"

/* 串口接收信号量：IDLE 中断 Give，接收任务 Take */
SemaphoreHandle_t xUartRxSem;

/* printf 互斥锁 */
static SemaphoreHandle_t PrintMutex;

/*
 * 串口接收 lwrb 环形缓冲区。
 * - 底层存储 s_lwrb_rx_storage 直接作为 DMA 目标地址，DMA 在其中循环写入。
 * - lwrb 的 size = DMA 缓冲区大小，capacity = size - 1。
 *   当消费者始终在读时，capacity 不会成为瓶颈；
 *   只有当消费者停摆超过一整个 DMA 周期时才会出现丢字节。
 */
static uint8_t  s_lwrb_rx_storage[UART_RX_BUF_SIZE];
lwrb_t          s_uart_rx_ring;

/* DMA 上一次 NDTR 值（Circular 模式位置跟踪） */
static uint16_t s_dma_last_ndtr = UART_RX_BUF_SIZE;

void BSP_USART_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

	/* 初始化 lwrb：底层存储直接作为 DMA 目标 */
	lwrb_init(&s_uart_rx_ring, s_lwrb_rx_storage, sizeof(s_lwrb_rx_storage));
	s_dma_last_ndtr = UART_RX_BUF_SIZE;

	/* ---- GPIO ---- */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Fast_Speed;

	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	/* ---- USART1 ---- */
	USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200u;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);

    /* ---- DMA2 Stream2 Channel4：USART1 RX ----
     * 目标地址直接指向 lwrb 的底层存储，DMA 在其中循环写入。 */
    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)s_lwrb_rx_storage;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = UART_RX_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_Init(DMA2_Stream2, &DMA_InitStructure);
    DMA_Cmd(DMA2_Stream2, ENABLE);

    /* USART1 数据请求连接到 DMA */
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    /* ---- NVIC ---- */
    /* 清除残留中断，防止 FreeRTOS 运行时触发 configASSERT */
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 只使能 IDLE 中断：DMA 搬运数据，帧结束后 IDLE 触发一次 */
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    /* 创建同步原语 */
    PrintMutex  = xSemaphoreCreateMutex();
    xUartRxSem  = xSemaphoreCreateBinary();
}

/*
 * IDLE 中断里调用：根据 NDTR 变化计算新到的字节数，提交到 lwrb，
 * 并 Give 信号量唤醒接收任务。
 */
void UART_NotifyRxFromISR(BaseType_t *pxHigherPriorityTaskWoken, uint16_t received)
{
    if (received == 0) {
        return;
    }

    /* lwrb_advance 会自动按 free 容量截断，溢出时多余的字节被丢弃 */
    (void)lwrb_advance(&s_uart_rx_ring, received);

    if (xUartRxSem != NULL) {
        xSemaphoreGiveFromISR(xUartRxSem, pxHigherPriorityTaskWoken);
    }
}

/*
 * 串口接收任务：等待 IDLE 中断信号，把 lwrb 里所有可读字节
 * 解析成以 \r\n 结尾的整行并回显。
 */
void UARTRxTask(void *pvParameters)
{
    uint8_t  line[UART_RX_BUF_SIZE];
    (void)pvParameters;

    SafePrintf("UART RX Task Started (lwrb)\r\n");

    while (1)
    {
        /* 阻塞等待 IDLE 中断唤醒 */
        if (xSemaphoreTake(xUartRxSem, portMAX_DELAY) != pdPASS) {
            continue;
        }

        /* 一次性把 lwrb 里所有可读字节搬到 line 缓冲；
         * lwrb_find / lwrb_skip 都按"虚拟连续"视图操作。 */
        while (lwrb_get_full(&s_uart_rx_ring) > 0)
        {
            lwrb_sz_t full = lwrb_get_full(&s_uart_rx_ring);
            lwrb_sz_t got;

            if (full >= sizeof(line)) {
                full = sizeof(line) - 1;
            }

            got = lwrb_read(&s_uart_rx_ring, line, full);
            line[got] = '\0';

            /* 简单按字符串回显 */
            SafePrintf("RX: %s", (char *)line);
        }
    }
}

/*
 * 供 USART1_IRQHandler 使用：把当前 NDTR 与上次记录的 NDTR 比对，
 * 计算这一帧新到的字节数，并更新 s_dma_last_ndtr。
 */
uint16_t UART_CalcRxDelta(uint16_t current_ndtr)
{
    uint16_t received;

    if (current_ndtr > s_dma_last_ndtr) {
        /* DMA 环绕（经过 0→BUF_SIZE） */
        received = s_dma_last_ndtr + (UART_RX_BUF_SIZE - current_ndtr);
    } else {
        received = s_dma_last_ndtr - current_ndtr;
    }

    s_dma_last_ndtr = current_ndtr;
    return received;
}

void UART_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void UART_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	xSemaphoreTake(PrintMutex, portMAX_DELAY);
	for (i = 0; i < Length; i ++)
	{
		UART_SendByte(Array[i]);
	}
	xSemaphoreGive(PrintMutex);
}

void UART_SendString(char *String)
{
	uint8_t i;
	xSemaphoreTake(PrintMutex, portMAX_DELAY);
	for (i = 0; String[i] != '\0'; i ++)
	{
		UART_SendByte(String[i]);
	}
	xSemaphoreGive(PrintMutex);
}

static uint32_t UART_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;
	}
	return Result;
}

void UART_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)
	{
		UART_SendByte(Number / UART_Pow(10, Length - i - 1) % 10 + '0');
	}
}

int fputc(int ch, FILE *f)
{
	UART_SendByte(ch);
	return ch;
}

/* 线程安全 printf */
void SafePrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    xSemaphoreTake(PrintMutex, portMAX_DELAY);
    vprintf(format, args);
    xSemaphoreGive(PrintMutex);
    va_end(args);
}
