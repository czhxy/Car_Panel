#include "lcd.h"
#include "stdlib.h"
#include "Delay.h"

//管理LCD重要参数
//默认为竖屏
_lcd_dev lcddev;

//画笔颜色,背景颜色
u16 POINT_COLOR = 0x0000,BACK_COLOR = 0xFFFF;
u16 DeviceCode;

//******************************************************************
//函数名称  LCD_WR_REG
//功能：    向液晶屏写16位指令
//输入参数：Reg:要写的指令值
//******************************************************************
void LCD_WR_REG(u16 data)
{
	LCD->LCD_REG = data;
}

//******************************************************************
//函数名称  LCD_WR_DATA
//功能：    向液晶屏写16位数据
//******************************************************************
void LCD_WR_DATA(u16 data)
{
	LCD->LCD_RAM = data;
}

//******************************************************************
//函数名称  LCD_DrawPoint_16Bit
//功能：    写入一个16位颜色
//******************************************************************
void LCD_DrawPoint_16Bit(u16 color)
{
	LCD_WR_DATA(color);
}

//******************************************************************
//函数名称  LCD_WriteReg
//功能：    写寄存器数据
//******************************************************************
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue)
{
	LCD->LCD_REG = LCD_Reg;
	LCD->LCD_RAM = LCD_RegValue;
}

//******************************************************************
//函数名称  LCD_WriteRAM_Prepare
//功能：    开始写GRAM
//******************************************************************
void LCD_WriteRAM_Prepare(void)
{
	LCD_WR_REG(lcddev.wramcmd);
}

//******************************************************************
//函数名称  LCD_DrawPoint
//功能：    在指定位置写入一个点的颜色
//******************************************************************
void LCD_DrawPoint(u16 x,u16 y)
{
	LCD_SetCursor(x,y);
	LCD_WR_DATA(POINT_COLOR);
}

//******************************************************************
//函数名称  LCD_Clear
//功能：    LCD全屏填充颜色
//******************************************************************
void LCD_Clear(u16 Color)
{
	u32 index = 0;
	LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
	for (index = 0; index < (u32)lcddev.width * lcddev.height; index++)
	{
		LCD->LCD_RAM = Color;
	}
}

//******************************************************************
//函数名称  LCD_GPIOInit
//功能：    液晶IO初始化，FSMC总线初始化
//******************************************************************
void LCD_GPIOInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  readWriteTiming;
	FSMC_NORSRAMTimingInitTypeDef  writeTiming;

	// F407: FSMC 时钟在 AHB3 总线上
	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);
	// F407: GPIO 时钟在 AHB1 总线上
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC |
						   RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE |
						   RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOG, ENABLE);

	// PB15 背光控制（推挽输出）
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// PF11 液晶复位脚（推挽输出）
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	// PORTD FSMC 复用推挽 (PD0,PD1,PD4,PD5,PD8,PD9,PD10,PD14,PD15)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
								  GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								  GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	// 设置 PD 口为 FSMC 复用功能
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource0,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource1,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource4,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource5,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource8,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource9,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_FSMC);

	// PORTE FSMC 复用推挽 (PE7~PE15)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7  | GPIO_Pin_8  | GPIO_Pin_9  |
								  GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 |
								  GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	// 设置 PE 口为 FSMC 复用功能
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource7,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource8,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource9,  GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, GPIO_AF_FSMC);

	// PORTG FSMC 复用推挽 (PG12 NE4, PF12 A6)
	// 注意：F407 的 FSMC_A6 在 PF12，不是 PG0
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOG, GPIO_PinSource12, GPIO_AF_FSMC);

	// PF12 作为 FSMC_A6 (RS) 复用推挽
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOF, GPIO_PinSource12, GPIO_AF_FSMC);

	// FSMC 时序配置（F407 @ 168MHz, 1 HCLK ≈ 5.95ns）
	readWriteTiming.FSMC_AddressSetupTime = 1;     // ADDSET = 1 HCLK ≈ 6ns
	readWriteTiming.FSMC_AddressHoldTime = 0;
	readWriteTiming.FSMC_DataSetupTime = 15;        // DATAST = 15 HCLK ≈ 89ns (ILI9486 min 80ns read)
	readWriteTiming.FSMC_BusTurnAroundDuration = 0;
	readWriteTiming.FSMC_CLKDivision = 0;
	readWriteTiming.FSMC_DataLatency = 0;
	readWriteTiming.FSMC_AccessMode = FSMC_AccessMode_A;

	writeTiming.FSMC_AddressSetupTime = 2;          // ADDSET = 2 HCLK ≈ 12ns
	writeTiming.FSMC_AddressHoldTime = 0;
	writeTiming.FSMC_DataSetupTime = 5;             // DATAST = 5 HCLK ≈ 30ns (ILI9486 min 15ns write)
	writeTiming.FSMC_BusTurnAroundDuration = 0;
	writeTiming.FSMC_CLKDivision = 0;
	writeTiming.FSMC_DataLatency = 0;
	writeTiming.FSMC_AccessMode = FSMC_AccessMode_A;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM4;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &readWriteTiming;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &writeTiming;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);
}

//******************************************************************
//函数名称  LCD_RESET
//功能：    LCD复位
//******************************************************************
void LCD_RESET(void)
{
	LCD_RST_CLR;
	Delay_ms(100);
	LCD_RST_SET;
	Delay_ms(50);
}

//******************************************************************
//函数名称  LCD_Init
//功能：    LCD初始化（ILI9486 FSMC模式）
//******************************************************************
void LCD_Init(void)
{
	LCD_GPIOInit();
	LCD_RESET();

	// ILI9486 初始化序列
	LCD_WR_REG(0xF1);
	LCD_WR_DATA(0x36);
	LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x3C);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x8F);
	LCD_WR_REG(0xF2);
	LCD_WR_DATA(0x18);
	LCD_WR_DATA(0xA3);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0xB2);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0xFF);
	LCD_WR_DATA(0x10);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xF8);
	LCD_WR_DATA(0x21);
	LCD_WR_DATA(0x04);
	LCD_WR_REG(0xF9);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x08);
	LCD_WR_REG(0x36);
	LCD_WR_DATA(0x08);
	LCD_WR_REG(0xB4);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0xB6);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x22);
	LCD_WR_REG(0xC1);
	LCD_WR_DATA(0x41);
	LCD_WR_REG(0xC5);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x18);
	// 正电压伽马校正
	LCD_WR_REG(0xE0);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x1F);
	LCD_WR_DATA(0x1C);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x08);
	LCD_WR_DATA(0x48);
	LCD_WR_DATA(0x98);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x13);
	LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x11);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x00);
	// 负电压伽马校正
	LCD_WR_REG(0xE1);
	LCD_WR_DATA(0x0F);
	LCD_WR_DATA(0x32);
	LCD_WR_DATA(0x2E);
	LCD_WR_DATA(0x0B);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x05);
	LCD_WR_DATA(0x47);
	LCD_WR_DATA(0x75);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x06);
	LCD_WR_DATA(0x10);
	LCD_WR_DATA(0x03);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x20);
	LCD_WR_DATA(0x00);
	// 像素格式: 16bpp 565
	LCD_WR_REG(0x3A);
	LCD_WR_DATA(0x55);
	// 退出休眠
	LCD_WR_REG(0x11);
	// 设置显示方向 BGR
	LCD_WR_REG(0x36);
	LCD_WR_DATA(0xC8);
	Delay_ms(120);
	// 开启显示
	LCD_WR_REG(0x29);

	LCD_SetParam();		//设置LCD参数
	LCD_BL_ON();		/* 点亮背光（PB15低电平点亮，板上Q2 PNP） */
	LCD_Clear(WHITE);
}

//******************************************************************
//函数名称  LCD_SetWindows
//功能：    设置显示窗口
//******************************************************************
void LCD_SetWindows(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd)
{
	LCD_WR_REG(lcddev.setxcmd);
	LCD_WR_DATA(xStar >> 8);
	LCD_WR_DATA(0x00FF & xStar);
	LCD_WR_DATA(xEnd >> 8);
	LCD_WR_DATA(0x00FF & xEnd);

	LCD_WR_REG(lcddev.setycmd);
	LCD_WR_DATA(yStar >> 8);
	LCD_WR_DATA(0x00FF & yStar);
	LCD_WR_DATA(yEnd >> 8);
	LCD_WR_DATA(0x00FF & yEnd);

	LCD_WriteRAM_Prepare();	//开始写入GRAM
}

//******************************************************************
//函数名称  LCD_SetCursor
//功能：    设置光标位置
//******************************************************************
void LCD_SetCursor(u16 Xpos, u16 Ypos)
{
	LCD_WR_REG(lcddev.setxcmd);
	LCD_WR_DATA(Xpos >> 8);
	LCD_WR_DATA(0x00FF & Xpos);
	LCD_WR_DATA((Xpos + 1) >> 8);
	LCD_WR_DATA((Xpos + 1));

	LCD_WR_REG(lcddev.setycmd);
	LCD_WR_DATA(Ypos >> 8);
	LCD_WR_DATA(0x00FF & Ypos);
	LCD_WR_DATA((Ypos + 1) >> 8);
	LCD_WR_DATA((Ypos + 1));
	LCD_WriteRAM_Prepare();	//开始写入GRAM
}

//******************************************************************
//函数名称  LCD_SetParam
//功能：    设置LCD参数（横竖屏切换）
//******************************************************************
void LCD_SetParam(void)
{
	lcddev.setxcmd = 0x2A;
	lcddev.setycmd = 0x2B;
	lcddev.wramcmd = 0x2C;
#if USE_HORIZONTAL == 1	//使用横屏
	lcddev.dir = 1;		//横屏
	lcddev.width = 480;
	lcddev.height = 320;
	LCD_WriteReg(0x36, (1 << 3) | (1 << 7) | (1 << 5));	//BGR=1,MY=1,MX=0,MV=1
#else					//竖屏
	lcddev.dir = 0;		//竖屏
	lcddev.width = 320;
	lcddev.height = 480;
	LCD_WriteReg(0x36, (1 << 3) | (1 << 6) | (1 << 7));	//BGR=1,MY=0,MX=0,MV=0
#endif
}

//******************************************************************
//函数名称  LCD_ReadPoint / LCD_RD_DATA / LCD_ReadReg / LCD_ReadRAM
//功能：    读取相关函数（暂不需要，保留空壳或后续实现）
//******************************************************************
u16 LCD_ReadPoint(u16 x, u16 y)
{
	(void)x; (void)y;
	return 0;
}

u16 LCD_RD_DATA(void)
{
	return 0;
}

u16 LCD_ReadReg(u8 LCD_Reg)
{
	(void)LCD_Reg;
	return 0;
}

u16 LCD_ReadRAM(void)
{
	return 0;
}

u16 LCD_BGR2RGB(u16 c)
{
	u16 r, g, b, rgb;
	b = (c >> 0)  & 0x1F;
	g = (c >> 5)  & 0x3F;
	r = (c >> 11) & 0x1F;
	rgb = (b << 11) + (g << 5) + (r << 0);
	return rgb;
}

void LCD_DisplayOn(void)
{
	LCD_BL_ON();  	/* 低电平点亮 */
}

void LCD_DisplayOff(void)
{
	LCD_BL_OFF();  	/* 高电平熄灭 */
}

//******************************************************************
//函数名称  LCD_DrawLine
//功能：    画线（Bresenham 算法简化版）
//******************************************************************
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
	u16 t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, row, col;
	delta_x = x2 - x1;
	delta_y = y2 - y1;
	row = x1;
	col = y1;
	if (delta_x > 0) incx = 1;
	else if (delta_x == 0) incx = 0;
	else { incx = -1; delta_x = -delta_x; }
	if (delta_y > 0) incy = 1;
	else if (delta_y == 0) incy = 0;
	else { incy = -1; delta_y = -delta_y; }
	distance = delta_x > delta_y ? delta_x : delta_y;
	for (t = 0; t <= distance + 1; t++)
	{
		LCD_DrawPoint(row, col);
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance) { xerr -= distance; row += incx; }
		if (yerr > distance) { yerr -= distance; col += incy; }
	}
}

//******************************************************************
//函数名称  LCD_DrawRectangle
//功能：    画矩形
//******************************************************************
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1, y1, x2, y1);
	LCD_DrawLine(x1, y1, x1, y2);
	LCD_DrawLine(x1, y2, x2, y2);
	LCD_DrawLine(x2, y1, x2, y2);
}
