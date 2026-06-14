#include "stm32f4xx.h"
#include "usart.h"

/**
 * @brief  Main program
 */
int main(void)
{
    UART_Init();
		printf("hello\r\n");
    while (1)
    {

    }
}


