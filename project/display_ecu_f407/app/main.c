/**
 * LCD 验证固件 —— 裸机版本（FreeRTOS 已注释）
 *
 * 验证流程:
 *   1. LED0 (PF9) 初始化
 *   2. LCD 初始化 (FSMC + ILI9486)
 *   3. LED 闪两下表示初始化完成
 *   4. 纯色刷屏测试：红→绿→蓝→白→黑
 *   5. 主循环 LED0 心跳闪烁
 */
#include "main.h"

/* ---- 简易 LED 控制宏（直接操作 PF9/PF10, 低电平点亮） ---- */
#define LED0_ON()   GPIOF->BSRR = (1 << (9 + 16))   /* PF9=0 点亮 */
#define LED0_OFF()  GPIOF->BSRR = (1 << 9)           /* PF9=1 熄灭 */
#define LED0_TOGGLE() do { \
	if (GPIOF->ODR & (1 << 9)) \
		GPIOF->BSRR = (1 << (9 + 16)); \
	else \
		GPIOF->BSRR = (1 << 9); \
} while(0)

static void LED_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	LED0_OFF();
}

/**
 * LCD 纯色刷屏测试：依次显示红/绿/蓝/白/黑
 */
static void LCD_ColorTest(void)
{
	LCD_Clear(RED);
	Delay_ms(500);
	LCD_Clear(GREEN);
	Delay_ms(500);
	LCD_Clear(BLUE);
	Delay_ms(500);
	LCD_Clear(WHITE);
	Delay_ms(500);
	LCD_Clear(BLACK);
	Delay_ms(500);
	LCD_Clear(WHITE);
}

int main()
{
	/* ---- LED 初始化 ---- */
	LED_Init();

	/* ---- LCD 初始化（FSMC + ILI9486） ---- */
	LCD_Init();

	/* ---- LED 闪烁 2 次，表示初始化完成 ---- */
	LED0_ON();   Delay_ms(200);
	LED0_OFF();  Delay_ms(200);
	LED0_ON();   Delay_ms(200);
	LED0_OFF();  Delay_ms(200);

	/* ---- LCD 纯色测试 ---- */
	LCD_ColorTest();

	/* ---- 主循环：LED0 心跳 ---- */
	while (1)
	{
		LED0_TOGGLE();
		Delay_ms(500);
	}

	/***************************************************
	 * 以下 FreeRTOS 代码暂时注释，等 LCD 验证通过后再恢复
	 ***************************************************
	BSP_USART_Init();
	SafePrintf("FreeRTOS Template (lwrb)\r\n");
	if (xUartRxSem != NULL) {
		xTaskCreate(UARTRxTask, "UART_RX", 256, NULL, 2, NULL);
	}
	xTaskCreate(LEDTask1,  "LED1", 256, NULL, 1, NULL);
	xTaskCreate(LEDTask2,  "LED2", 256, NULL, 1, NULL);
	xTaskCreate(KEYTask,   "KEY",  256, NULL, 2, NULL);
	vTaskStartScheduler();
	while (1) {}
	***************************************************/
}
