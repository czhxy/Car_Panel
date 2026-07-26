#ifndef __BSP_SPI_LCD_H
#define __BSP_SPI_LCD_H

#include "stm32f4xx.h"

/* ============================================================
 * 引脚定义
 * ============================================================ */
/* SPI5 引脚 */
#define LCD_SPI              SPI5
#define LCD_SPI_SCK_PIN      GPIO_Pin_7
#define LCD_SPI_SCK_PORT     GPIOF
#define LCD_SPI_MISO_PIN     GPIO_Pin_8
#define LCD_SPI_MISO_PORT    GPIOF
#define LCD_SPI_MOSI_PIN     GPIO_Pin_9
#define LCD_SPI_MOSI_PORT    GPIOF
#define LCD_SPI_GPIO_CLK     RCC_AHB1Periph_GPIOF
#define LCD_SPI_CLK          RCC_APB2Periph_SPI5
#define LCD_SPI_AF            GPIO_AF_SPI5

/* LCD 控制引脚 */
#define LCD_RS_PIN           GPIO_Pin_8
#define LCD_RS_PORT          GPIOI
#define LCD_RST_PIN          GPIO_Pin_9
#define LCD_RST_PORT         GPIOI
#define LCD_CS_PIN           GPIO_Pin_10
#define LCD_CS_PORT          GPIOI
#define LCD_CTL_GPIO_CLK     RCC_AHB1Periph_GPIOI

/* 背光引脚 */
#define LCD_BL_PIN           GPIO_Pin_6
#define LCD_BL_PORT          GPIOD
#define LCD_BL_GPIO_CLK      RCC_AHB1Periph_GPIOD

/* LCD 控制宏（直接操作寄存器，高速） */
#define LCD_CS_SET           (LCD_CS_PORT->BSRR = LCD_CS_PIN)
#define LCD_RS_SET           (LCD_RS_PORT->BSRR = LCD_RS_PIN)
#define LCD_RST_SET          (LCD_RST_PORT->BSRR = LCD_RST_PIN)

#define LCD_CS_CLR           (LCD_CS_PORT->BSRR = (uint32_t)LCD_CS_PIN << 16)
#define LCD_RS_CLR           (LCD_RS_PORT->BSRR = (uint32_t)LCD_RS_PIN << 16)
#define LCD_RST_CLR          (LCD_RST_PORT->BSRR = (uint32_t)LCD_RST_PIN << 16)

#define LCD_BL_ON()          (LCD_BL_PORT->BSRR = LCD_BL_PIN)
#define LCD_BL_OFF()         (LCD_BL_PORT->BSRR = (uint32_t)LCD_BL_PIN << 16)

/* ============================================================
 * LCD 参数结构体
 * ============================================================ */
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t id;
    uint8_t  dir;
    uint16_t wramcmd;
    uint16_t rramcmd;
    uint16_t setxcmd;
    uint16_t setycmd;
} _lcd_dev;

extern _lcd_dev lcddev;

/* 屏幕方向：0-竖屏 1-横屏 */
#define USE_HORIZONTAL  0

#define LCD_W 240
#define LCD_H 320

/* 全局画笔/背景色 */
extern uint16_t POINT_COLOR;
extern uint16_t BACK_COLOR;

/* ============================================================
 * 颜色定义（RGB565）
 * ============================================================ */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define BRED        0xF81F
#define GRED        0xFFE0
#define GBLUE       0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define BROWN       0xBC40
#define BRRED       0xFC07
#define GRAY        0x8430

#define DARKBLUE    0x01CF
#define LIGHTBLUE   0x7D7C
#define GRAYBLUE    0x5458
#define LIGHTGREEN  0x841F
#define LIGHTGRAY   0xEF5B
#define LGRAY       0xC618
#define LGRAYBLUE   0xA651
#define LBBLUE      0x2B12

/* ============================================================
 * API 声明
 * ============================================================ */
void BSP_SPI_LCD_Init(void);

/* SPI 底层操作 */
uint8_t SPI_WriteByte(uint8_t TxData);
void SPI_SetSpeed(uint8_t SPI_BaudRatePrescaler);

/* LCD 寄存器/数据操作 */
void LCD_WR_REG(uint8_t data);
void LCD_WR_DATA(uint8_t data);
uint8_t LCD_RD_DATA(void);
void LCD_WriteReg(uint8_t LCD_Reg, uint16_t LCD_RegValue);
uint8_t LCD_ReadReg(uint8_t LCD_Reg);
void LCD_WriteRAM_Prepare(void);
void LCD_ReadRAM_Prepare(void);
void Lcd_WriteData_16Bit(uint16_t Data);
uint16_t Lcd_ReadData_16Bit(void);
uint16_t Color_To_565(uint8_t r, uint8_t g, uint8_t b);

/* LCD 绘图函数 */
void LCD_Clear(uint16_t Color);
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos);
void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd);
void LCD_DrawPoint(uint16_t x, uint16_t y);
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_direction(uint8_t direction);
uint16_t LCD_Read_ID(void);

#endif /* __BSP_SPI_LCD_H */
