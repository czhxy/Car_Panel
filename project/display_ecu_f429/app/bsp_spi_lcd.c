#include "bsp_spi_lcd.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * 全局变量
 * ============================================================ */
_lcd_dev lcddev;
uint16_t POINT_COLOR = 0x0000;
uint16_t BACK_COLOR  = 0xFFFF;
/* ============================================================
 * SPI5 硬件初始化（SPL）
 * ============================================================ */
static void SPI5_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(LCD_SPI_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(LCD_SPI_CLK, ENABLE);

    /* PF7=SCK, PF8=MISO, PF9=MOSI */
    GPIO_InitStructure.GPIO_Pin   = LCD_SPI_SCK_PIN | LCD_SPI_MISO_PIN | LCD_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOF, GPIO_PinSource7, LCD_SPI_AF);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, LCD_SPI_AF);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, LCD_SPI_AF);
}

static void SPI5_Config(void)
{
    SPI_InitTypeDef SPI_InitStructure;

    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(LCD_SPI, &SPI_InitStructure);

    SPI_Cmd(LCD_SPI, ENABLE);
}

/* ============================================================
 * SPI 字节收发（轮询模式）
 * ============================================================ */
uint8_t SPI_WriteByte(uint8_t TxData)
{
    while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(LCD_SPI, TxData);
    while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(LCD_SPI);
}

void SPI_SetSpeed(uint8_t SPI_BaudRatePrescaler)
{
    SPI_Cmd(LCD_SPI, DISABLE);
    LCD_SPI->CR1 &= 0xFFC7;
    LCD_SPI->CR1 |= SPI_BaudRatePrescaler;
    SPI_Cmd(LCD_SPI, ENABLE);
}

/* ============================================================
 * LCD GPIO 初始化
 * ============================================================ */
static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 背光引脚 PD6 */
    RCC_AHB1PeriphClockCmd(LCD_BL_GPIO_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = LCD_BL_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(LCD_BL_PORT, &GPIO_InitStructure);

    /* LCD 控制引脚 PI8=RS, PI9=RST, PI10=CS */
    RCC_AHB1PeriphClockCmd(LCD_CTL_GPIO_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = LCD_RS_PIN | LCD_RST_PIN | LCD_CS_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure);

    LCD_BL_ON();
}

/* ============================================================
 * LCD 硬件复位
 * ============================================================ */
static void LCD_RESET(void)
{
    LCD_RST_SET;
    vTaskDelay(pdMS_TO_TICKS(50));
    LCD_RST_CLR;
    vTaskDelay(pdMS_TO_TICKS(100));
    LCD_RST_SET;
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ============================================================
 * LCD 基础操作
 * ============================================================ */
void LCD_WR_REG(uint8_t data)
{
    LCD_CS_CLR;
    LCD_RS_CLR;
    SPI_WriteByte(data);
    LCD_CS_SET;
}

void LCD_WR_DATA(uint8_t data)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    SPI_WriteByte(data);
    LCD_CS_SET;
}

uint8_t LCD_RD_DATA(void)
{
    uint8_t data;
    LCD_CS_CLR;
    LCD_RS_SET;
    SPI_SetSpeed(SPI_BaudRatePrescaler_16);
    data = SPI_WriteByte(0xFF);
    SPI_SetSpeed(SPI_BaudRatePrescaler_2);
    LCD_CS_SET;
    return data;
}

void LCD_WriteReg(uint8_t LCD_Reg, uint16_t LCD_RegValue)
{
    LCD_WR_REG(LCD_Reg);
    LCD_WR_DATA(LCD_RegValue);
}

uint8_t LCD_ReadReg(uint8_t LCD_Reg)
{
    LCD_WR_REG(LCD_Reg);
    return LCD_RD_DATA();
}

void LCD_WriteRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.wramcmd);
}

void LCD_ReadRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.rramcmd);
}

void Lcd_WriteData_16Bit(uint16_t Data)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    SPI_WriteByte(Data >> 8);
    SPI_WriteByte(Data);
    LCD_CS_SET;
}

uint16_t Color_To_565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

uint16_t Lcd_ReadData_16Bit(void)
{
    uint8_t r, g, b;
    LCD_RS_SET;
    LCD_CS_CLR;
    SPI_SetSpeed(SPI_BaudRatePrescaler_16);
    SPI_WriteByte(0xFF);
    r = SPI_WriteByte(0xFF);
    g = SPI_WriteByte(0xFF);
    b = SPI_WriteByte(0xFF);
    SPI_SetSpeed(SPI_BaudRatePrescaler_2);
    LCD_CS_SET;
    return Color_To_565(r, g, b);
}

/* ============================================================
 * LCD 绘图函数
 * ============================================================ */
void LCD_DrawPoint(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    Lcd_WriteData_16Bit(POINT_COLOR);
}

uint16_t LCD_ReadPoint(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    LCD_ReadRAM_Prepare();
    return Lcd_ReadData_16Bit();
}

void LCD_Clear(uint16_t Color)
{
    uint32_t i, m;
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;

    LCD_SetWindows(0, 0, w - 1, h - 1);
    LCD_CS_CLR;
    LCD_RS_SET;
    for (i = 0; i < h; i++)
    {
        for (m = 0; m < w; m++)
        {
            LCD_SPI->DR = Color >> 8;
            while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);
            while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
            (void)SPI_I2S_ReceiveData(LCD_SPI);

            LCD_SPI->DR = (uint8_t)Color;
            while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);
            while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
            (void)SPI_I2S_ReceiveData(LCD_SPI);
        }
    }
    LCD_CS_SET;
}

void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{
    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(xStar >> 8);
    LCD_WR_DATA((uint8_t)(xStar & 0xFF));
    LCD_WR_DATA(xEnd >> 8);
    LCD_WR_DATA((uint8_t)(xEnd & 0xFF));

    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(yStar >> 8);
    LCD_WR_DATA((uint8_t)(yStar & 0xFF));
    LCD_WR_DATA(yEnd >> 8);
    LCD_WR_DATA((uint8_t)(yEnd & 0xFF));

    LCD_WriteRAM_Prepare();
}

void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
    LCD_SetWindows(Xpos, Ypos, Xpos, Ypos);
}

void LCD_direction(uint8_t direction)
{
    lcddev.setxcmd = 0x2A;
    lcddev.setycmd = 0x2B;
    lcddev.wramcmd = 0x2C;
    lcddev.rramcmd = 0x2E;

    switch (direction)
    {
    case 0:
        lcddev.width  = LCD_W;
        lcddev.height = LCD_H;
        LCD_WriteReg(0x36, (1 << 3) | (0 << 5) | (0 << 6) | (0 << 7));
        break;
    case 1:
        lcddev.width  = LCD_H;
        lcddev.height = LCD_W;
        LCD_WriteReg(0x36, (1 << 3) | (1 << 5) | (1 << 6) | (0 << 7));
        break;
    case 2:
        lcddev.width  = LCD_W;
        lcddev.height = LCD_H;
        LCD_WriteReg(0x36, (1 << 3) | (0 << 5) | (1 << 6) | (1 << 7));
        break;
    case 3:
        lcddev.width  = LCD_H;
        lcddev.height = LCD_W;
        LCD_WriteReg(0x36, (1 << 3) | (1 << 5) | (0 << 6) | (1 << 7));
        break;
    default: break;
    }
    lcddev.dir = direction % 4;
}

uint16_t LCD_Read_ID(void)
{
    uint8_t i, val[3] = {0};
    for (i = 1; i < 4; i++)
    {
        LCD_WR_REG(0xD9);
        LCD_WR_DATA(0x10 + i);
        LCD_WR_REG(0xD3);
        val[i - 1] = LCD_RD_DATA();
    }
    lcddev.id  = val[1];
    lcddev.id <<= 8;
    lcddev.id  |= val[2];
    return lcddev.id;
}

/* ============================================================
 * BSP_SPI_LCD_Init — SPI5 + ILI9341 全链路初始化
 * 调用时机：Task_Entry_All 中，调度器已运行，vTaskDelay 可用
 * ============================================================ */
void BSP_SPI_LCD_Init(void)
{
    LOG_I("[LCD] Initializing SPI5 + ILI9341...\r\n");

    SPI5_GPIO_Init();
    SPI5_Config();
    LCD_GPIO_Init();
    LCD_RESET();

    /* ILI9341 初始化序列（2.8寸 IPS） */
    LCD_WR_REG(0xCF);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0xC1);
    LCD_WR_DATA(0x30);

    LCD_WR_REG(0xED);
    LCD_WR_DATA(0x64);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0X12);
    LCD_WR_DATA(0X81);

    LCD_WR_REG(0xE8);
    LCD_WR_DATA(0x85);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x78);

    LCD_WR_REG(0xCB);
    LCD_WR_DATA(0x39);
    LCD_WR_DATA(0x2C);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x34);
    LCD_WR_DATA(0x02);

    LCD_WR_REG(0xF7);
    LCD_WR_DATA(0x20);

    LCD_WR_REG(0xEA);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xC0);
    LCD_WR_DATA(0x13);

    LCD_WR_REG(0xC1);
    LCD_WR_DATA(0x13);

    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x22);
    LCD_WR_DATA(0x35);

    LCD_WR_REG(0xC7);
    LCD_WR_DATA(0xBD);

    LCD_WR_REG(0x21);

    LCD_WR_REG(0x36);
    LCD_WR_DATA(0x08);

    LCD_WR_REG(0xB6);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0xA2);

    LCD_WR_REG(0x3A);
    LCD_WR_DATA(0x55);

    LCD_WR_REG(0xF6);
    LCD_WR_DATA(0x01);
    LCD_WR_DATA(0x30);

    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x1B);

    LCD_WR_REG(0xF2);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0x26);
    LCD_WR_DATA(0x01);

    LCD_WR_REG(0xE0);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x35);
    LCD_WR_DATA(0x31);
    LCD_WR_DATA(0x0B);
    LCD_WR_DATA(0x0E);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x49);
    LCD_WR_DATA(0xA7);
    LCD_WR_DATA(0x33);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0XE1);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x0F);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x11);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x58);
    LCD_WR_DATA(0x4D);
    LCD_WR_DATA(0x07);
    LCD_WR_DATA(0x10);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x32);
    LCD_WR_DATA(0x34);
    LCD_WR_DATA(0x0F);

    LCD_WR_REG(0x11);               /* Exit Sleep */
    vTaskDelay(pdMS_TO_TICKS(120));
    LCD_WR_REG(0x29);               /* Display on */

    LCD_direction(USE_HORIZONTAL);  /* 设置显示方向 */
    LCD_Clear(WHITE);               /* 清屏为白色 */

    LOG_I("[LCD] ILI9341 Init done, W=%d H=%d\r\n", lcddev.width, lcddev.height);
}
