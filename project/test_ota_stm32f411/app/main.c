#include "stm32f4xx.h"
#include "usart.h"

int main(void)
{
    UART_Init();
    printf("[APP] VTOR=0x%08X  RCC_CSR=0x%08X\r\n",
           (unsigned int)SCB->VTOR, (unsigned int)RCC->CSR);
    printf("hello\r\n");
    while (1)
    {
    }
}


