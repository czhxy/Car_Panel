#ifndef __LCD_H
#define __LCD_H
#include "stm32f4xx.h"
#include "stdlib.h"

/* ==================== 类型别名 ==================== */
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint8_t  u8;

//=======================================液晶屏数据线连接==========================================//
//DB0       → PD14
//DB1       → PD15
//DB2       → PD0
//DB3       → PD1
//DB4~DB12  → PE7~PE15
//DB13      → PD8
//DB14      → PD9
//DB15      → PD10
//=======================================液晶屏控制线连接==========================================//
//LCD_CS    → PG12   (FSMC_NE4)  片选
//LCD_RS    → PF12   (FSMC_A6)   寄存器/数据选择（原理图用A6非A0）
//LCD_WR    → PD5    (FSMC_NWE)  写信号
//LCD_RD    → PD4    (FSMC_NOE)  读信号
//LCD_RST   → PF11   复位信号
//LCD_LED   → PB15   背光控制(板上Q2 PNP, 低电平点亮)

//LCD重要参数
typedef struct
{
	u16 width;			//LCD 宽度
	u16 height;			//LCD 高度
	u16 id;				//LCD ID
	u8  dir;			//横屏/竖屏控制：0，竖屏；1，横屏
	u16	 wramcmd;		//开始写GRAM指令
	u16  setxcmd;		//设置X坐标指令
	u16  setycmd;		//设置Y坐标指令
}_lcd_dev;

//LCD参数
extern _lcd_dev lcddev;	//管理LCD重要参数
/////////////////////////////////////用户配置///////////////////////////////////
//支持横竖屏快速切换
#define USE_HORIZONTAL  	0	//定义是否使用横屏 		0,不使用.1,使用.

//LCD地址结构体
typedef struct
{
	u16 LCD_REG;
	u16 LCD_RAM;
} LCD_TypeDef;

//FSMC Bank1 NORSRAM4 (NE4=PG12), A6作RS
//16位数据宽度下 A6=HADDR[7], 基地址 +0 时 A6=0(命令), 基地址 +2 时 addr=0x80 -> A6=1(数据)
#define LCD_BASE        ((u32)(0x6C000000 | 0x0000007E))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

//TFTLCD全局颜色变量
extern u16  POINT_COLOR;	//默认画笔颜色
extern u16  BACK_COLOR; 	//背景颜色,默认为白色

////////////////////////////////////////////////////////////////////
//-----------------LCD端口定义----------------
// PB15 背光控制（板上 Q2 PNP，低电平点亮）
#define LCD_BL_ON()   GPIOB->BSRR = (1 << (15 + 16))   /* PB15=0, BSRR[31:16] 复位 */
#define LCD_BL_OFF()  GPIOB->BSRR = (1 << 15)           /* PB15=1, BSRR[15:0] 置位 */

// 快速IO翻转（操作 BSRR 寄存器）
#define	LCD_RST_SET	GPIOF->BSRR = (1 << 11)            /* PF11=1, BSRR[15:0] 置位 */
#define	LCD_RST_CLR	GPIOF->BSRR = (1 << (11 + 16))     /* PF11=0, BSRR[31:16] 复位 */

//////////////////////////////////////////////////////////////////////

//扫描方向
#define L2R_U2D  0 //从左到右,从上到下
#define L2R_D2U  1 //从左到右,从下到上
#define R2L_U2D  2 //从右到左,从上到下
#define R2L_D2U  3 //从右到左,从下到上

#define U2D_L2R  4 //从上到下,从左到右
#define U2D_R2L  5 //从上到下,从右到左
#define D2U_L2R  6 //从下到上,从左到右
#define D2U_R2L  7 //从下到上,从右到左

#define DFT_SCAN_DIR  L2R_U2D  //默认的扫描方向

//画笔颜色
#define WHITE       0xFFFF
#define BLACK      	0x0000
#define BLUE       	0x001F
#define BRED        0XF81F
#define GRED 			 	0XFFE0
#define GBLUE			 	0X07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define BROWN 			0XBC40
#define BRRED 			0XFC07
#define GRAY  			0X8430
//GUI颜色

#define DARKBLUE      	 0X01CF
#define LIGHTBLUE      	 0X7D7C
#define GRAYBLUE       	 0X5458

#define LIGHTGREEN     	0X841F
#define LGRAY 			 		0XC618

#define LGRAYBLUE      	0XA651
#define LBBLUE          0X2B12

extern u16 BACK_COLOR, POINT_COLOR ;

void LCD_Init(void);
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);
void LCD_Clear(u16 Color);
void LCD_SetCursor(u16 Xpos, u16 Ypos);
void LCD_DrawPoint(u16 x,u16 y);
u16  LCD_ReadPoint(u16 x,u16 y);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_SetWindows(u16 xStar, u16 yStar,u16 xEnd,u16 yEnd);
void LCD_DrawPoint_16Bit(u16 color);
u16 LCD_RD_DATA(void);
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
void LCD_WR_DATA(u16 data);
u16 LCD_ReadReg(u8 LCD_Reg);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);
u16 LCD_ReadRAM(void);
u16 LCD_BGR2RGB(u16 c);
void LCD_SetParam(void);

#endif
