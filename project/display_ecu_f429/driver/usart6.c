/* usart6.c — USART6 (PC6/PC7) 临时调试串口驱动
 *
 * 用途：VOFA+ firewater 波形数据专用输出口（当前发 RPM）。
 * 与 USART1（printf 日志 + 查询协议）完全隔离，避免波形数据混入日志。
 * 注意：仅发送，无接收需求；波特率 115200 与 VOFA+ 默认一致。
 * 测试完成后整个文件可删除（连同 usart6.h、task_vofa.c、mdk 工程引用）。
 */

#include "usart6.h"

/* ---- USART6 初始化：PC6=TX / PC7=RX，AF8，APB2 时钟 ---- */
void UART6_Init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);  /* F42x GPIO AF 需 SYSCFG */

    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Fast_Speed;
    GPIO_Init(GPIOC, &gpio);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_USART6);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_USART6);

    USART_InitTypeDef uart;
    USART_StructInit(&uart);
    uart.USART_BaudRate            = 115200;
    uart.USART_WordLength          = USART_WordLength_8b;
    uart.USART_StopBits            = USART_StopBits_1;
    uart.USART_Parity              = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART6, &uart);
    USART_Cmd(USART6, ENABLE);
}

void UART6_SendByte(uint8_t Byte)
{
    while (USART_GetFlagStatus(USART6, USART_FLAG_TXE) == RESET);
    USART_SendData(USART6, Byte);
}

void UART6_SendString(const char *String)
{
    size_t i;
    for (i = 0; String[i] != '\0'; i++) {
        UART6_SendByte((uint8_t)String[i]);
    }
}
