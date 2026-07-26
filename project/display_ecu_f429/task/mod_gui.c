#include "mod_gui.h"
#include "bsp_spi_lcd.h"
#include "bsp_spi_lcd_font.h"
#include "string.h"

/* ============================================================
 * GUI_DrawPoint — 绘制单点
 * ============================================================ */
void GUI_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetCursor(x, y);
    Lcd_WriteData_16Bit(color);
}

/* ============================================================
 * LCD_Fill — 填充矩形区域
 * ============================================================ */
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t i, j;
    uint16_t width  = ex - sx + 1;
    uint16_t height = ey - sy + 1;

    LCD_SetWindows(sx, sy, ex, ey);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            Lcd_WriteData_16Bit(color);
        }
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}

/* ============================================================
 * LCD_DrawPoint (使用 POINT_COLOR) — 兼容旧 API
 * ============================================================ */
void LCD_DrawPoint2(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    Lcd_WriteData_16Bit(POINT_COLOR);
}

/* ============================================================
 * LCD_DrawLine — Bresenham 画线
 * ============================================================ */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;

    if (delta_x > 0)      incx = 1;
    else if (delta_x == 0) incx = 0;
    else                  { incx = -1; delta_x = -delta_x; }

    if (delta_y > 0)      incy = 1;
    else if (delta_y == 0) incy = 0;
    else                  { incy = -1; delta_y = -delta_y; }

    if (delta_x > delta_y) distance = delta_x;
    else                   distance = delta_y;

    for (t = 0; t <= distance + 1; t++)
    {
        LCD_DrawPoint2(uRow, uCol);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/* ============================================================
 * LCD_DrawLine2 — 带线宽画线
 * ============================================================ */
void LCD_DrawLine2(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t size, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;

    if (delta_x > 0)      incx = 1;
    else if (delta_x == 0) incx = 0;
    else                  { incx = -1; delta_x = -delta_x; }

    if (delta_y > 0)      incy = 1;
    else if (delta_y == 0) incy = 0;
    else                  { incy = -1; delta_y = -delta_y; }

    if (delta_x > delta_y) distance = delta_x;
    else                   distance = delta_y;

    for (t = 0; t <= distance + 1; t++)
    {
        gui_circle(uRow, uCol, color, size, 1);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/* ============================================================
 * LCD_DrawRectangle — 绘制矩形边框
 * ============================================================ */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_DrawLine(x1, y1, x2, y1);
    LCD_DrawLine(x1, y1, x1, y2);
    LCD_DrawLine(x1, y2, x2, y2);
    LCD_DrawLine(x2, y1, x2, y2);
}

/* ============================================================
 * LCD_DrawFillRectangle — 填充矩形
 * ============================================================ */
void LCD_DrawFillRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_Fill(x1 + 1, y1 + 1, x2 - 1, y2 - 1, POINT_COLOR);
}

/* ============================================================
 * 圆角矩形辅助
 * ============================================================ */
static void Draw_Circle_Helper(uint16_t x0, uint16_t y0, uint16_t c, uint8_t cn)
{
    int16_t f     = 1 - c;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * c;
    uint16_t x    = 0;
    uint16_t y    = c;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cn & 0x4)
        {
            LCD_DrawPoint2(x0 + x, y0 + y);
            LCD_DrawPoint2(x0 + y, y0 + x);
        }
        if (cn & 0x2)
        {
            LCD_DrawPoint2(x0 + x, y0 - y);
            LCD_DrawPoint2(x0 + y, y0 - x);
        }
        if (cn & 0x8)
        {
            LCD_DrawPoint2(x0 - y, y0 + x);
            LCD_DrawPoint2(x0 - x, y0 + y);
        }
        if (cn & 0x1)
        {
            LCD_DrawPoint2(x0 - y, y0 - x);
            LCD_DrawPoint2(x0 - x, y0 - y);
        }
    }
}

static void Fill_Circle_Helper(uint16_t x0, uint16_t y0, uint16_t c, uint8_t cn, uint16_t delta)
{
    int16_t f     = 1 - c;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * c;
    uint16_t x    = 0;
    uint16_t y    = c;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cn & 0x1)
        {
            LCD_DrawLine(x0 + x, y0 - y, x0 + x, y0 + y + 1 + delta);
            LCD_DrawLine(x0 + y, y0 - x, x0 + y, y0 + x + 1 + delta);
        }
        if (cn & 0x2)
        {
            LCD_DrawLine(x0 - x, y0 - y, x0 - x, y0 + y + 1 + delta);
            LCD_DrawLine(x0 - y, y0 - x, x0 - y, y0 + x + 1 + delta);
        }
    }
}

void LCD_DrawRoundRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t c)
{
    LCD_DrawLine(x1 + c, y1, x2 - c, y1);
    LCD_DrawLine(x1, y1 + c, x1, y2 - c);
    LCD_DrawLine(x1 + c, y2, x2 - c, y2);
    LCD_DrawLine(x2, y1 + c, x2, y2 - c);
    Draw_Circle_Helper(x1 + c, y1 + c, c, 1);
    Draw_Circle_Helper(x2 - c, y1 + c, c, 2);
    Draw_Circle_Helper(x2 - c, y2 - c, c, 4);
    Draw_Circle_Helper(x1 + c, y2 - c, c, 8);
}

void LCD_FillRoundRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t c)
{
    LCD_Fill(x1 + c, y1, x2 - c, y2 + 1, POINT_COLOR);
    Fill_Circle_Helper(x2 - c, y1 + c, c, 1, (y2 - y1 + 1) - 2 * c - 1);
    Fill_Circle_Helper(x1 + c, y1 + c, c, 2, (y2 - y1 + 1) - 2 * c - 1);
}

/* ============================================================
 * gui_circle — Bresenham 画圆（支持填充）
 * ============================================================ */
static void _draw_circle_8(int xc, int yc, int x, int y, uint16_t c)
{
    GUI_DrawPoint(xc + x, yc + y, c);
    GUI_DrawPoint(xc - x, yc + y, c);
    GUI_DrawPoint(xc + x, yc - y, c);
    GUI_DrawPoint(xc - x, yc - y, c);
    GUI_DrawPoint(xc + y, yc + x, c);
    GUI_DrawPoint(xc - y, yc + x, c);
    GUI_DrawPoint(xc + y, yc - x, c);
    GUI_DrawPoint(xc - y, yc - x, c);
}

void gui_circle(int xc, int yc, uint16_t c, int r, int fill)
{
    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;

    if (fill)
    {
        while (x <= y)
        {
            for (yi = x; yi <= y; yi++)
                _draw_circle_8(xc, yc, x, yi, c);
            if (d < 0)
                d = d + 4 * x + 6;
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
    else
    {
        while (x <= y)
        {
            _draw_circle_8(xc, yc, x, y, c);
            if (d < 0)
                d = d + 4 * x + 6;
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}

/* ============================================================
 * Draw_Circle (对齐参考 API)
 * ============================================================ */
void Draw_Circle(uint16_t x0, uint16_t y0, uint16_t fc, uint8_t r)
{
    int x = 0, y = r, d;
    d = 3 - 2 * r;
    while (x <= y)
    {
        _draw_circle_8(x0, y0, x, y, fc);
        if (d < 0)
            d = d + 4 * x + 6;
        else
        {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/* ============================================================
 * Draw_Triangel / Fill_Triangel
 * ============================================================ */
void Draw_Triangel(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_DrawLine(x0, y0, x1, y1);
    LCD_DrawLine(x1, y1, x2, y2);
    LCD_DrawLine(x2, y2, x0, y0);
}

static void _swap(uint16_t *a, uint16_t *b)
{
    uint16_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void Fill_Triangel(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint16_t a, b, y, last;
    int dx01, dy01, dx02, dy02, dx12, dy12;
    long sa = 0;
    long sb = 0;

    if (y0 > y1) { _swap(&y0, &y1); _swap(&x0, &x1); }
    if (y1 > y2) { _swap(&y2, &y1); _swap(&x2, &x1); }
    if (y0 > y1) { _swap(&y0, &y1); _swap(&x0, &x1); }

    if (y0 == y2)
    {
        a = b = x0;
        if (x1 < a) a = x1;
        else if (x1 > b) b = x1;
        if (x2 < a) a = x2;
        else if (x2 > b) b = x2;
        LCD_Fill(a, y0, b, y0, POINT_COLOR);
        return;
    }

    dx01 = x1 - x0; dy01 = y1 - y0;
    dx02 = x2 - x0; dy02 = y2 - y0;
    dx12 = x2 - x1; dy12 = y2 - y1;

    if (y1 == y2) last = y1;
    else          last = y1 - 1;

    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) _swap(&a, &b);
        LCD_Fill(a, y, b, y, POINT_COLOR);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) _swap(&a, &b);
        LCD_Fill(a, y, b, y, POINT_COLOR);
    }
}

/* ============================================================
 * LCD_ShowChar — 显示 ASCII 字符
 * ============================================================ */
void LCD_ShowChar(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t num, uint8_t size, uint8_t mode)
{
    uint8_t temp;
    uint8_t pos, t;
    uint16_t colortemp = POINT_COLOR;

    num = num - ' ';  /* 偏移量 */
    LCD_SetWindows(x, y, x + size / 2 - 1, y + size - 1);

    if (!mode)  /* 非叠加模式 */
    {
        for (pos = 0; pos < size; pos++)
        {
            if (size == 12) temp = asc2_1206[num][pos];
            else            temp = asc2_1608[num][pos];

            for (t = 0; t < size / 2; t++)
            {
                if (temp & 0x01) Lcd_WriteData_16Bit(fc);
                else             Lcd_WriteData_16Bit(bc);
                temp >>= 1;
            }
        }
    }
    else  /* 叠加模式 */
    {
        for (pos = 0; pos < size; pos++)
        {
            if (size == 12) temp = asc2_1206[num][pos];
            else            temp = asc2_1608[num][pos];

            for (t = 0; t < size / 2; t++)
            {
                if (temp & 0x01) GUI_DrawPoint(x + t, y + pos, fc);
                temp >>= 1;
            }
        }
    }

    POINT_COLOR = colortemp;
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}

/* ============================================================
 * LCD_ShowString — 显示 ASCII 字符串
 * ============================================================ */
void LCD_ShowString(uint16_t x, uint16_t y, uint8_t size, uint8_t *p, uint8_t mode)
{
    while ((*p <= '~') && (*p >= ' '))
    {
        if (x > (lcddev.width - 1) || y > (lcddev.height - 1))
            return;
        LCD_ShowChar(x, y, POINT_COLOR, BACK_COLOR, *p, size, mode);
        x += size / 2;
        p++;
    }
}

/* ============================================================
 * mypow — 幂函数
 * ============================================================ */
static uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

/* ============================================================
 * LCD_ShowNum — 显示数字
 * ============================================================ */
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + (size / 2) * t, y, POINT_COLOR, BACK_COLOR, ' ', size, 0);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        LCD_ShowChar(x + (size / 2) * t, y, POINT_COLOR, BACK_COLOR, temp + '0', size, 0);
    }
}

void LCD_Show2Num(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint8_t size, uint8_t mode)
{
    (void)mode;
    LCD_ShowNum(x, y, num, len, size);
}

/* ============================================================
 * GUI_DrawFont16/24/32 — 显示中文字符
 * ============================================================ */
void GUI_DrawFont16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s, uint8_t mode)
{
    uint8_t i, j;
    uint16_t k;
    uint16_t HZnum;
    uint16_t x0 = x;

    HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
        {
            LCD_SetWindows(x, y, x + 16 - 1, y + 16 - 1);
            for (i = 0; i < 16 * 2; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)
                    {
                        if (tfont16[k].Msk[i] & (0x80 >> j))
                            Lcd_WriteData_16Bit(fc);
                        else
                            Lcd_WriteData_16Bit(bc);
                    }
                    else
                    {
                        if (tfont16[k].Msk[i] & (0x80 >> j))
                            GUI_DrawPoint(x, y, fc);
                        x++;
                        if ((x - x0) == 16)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            continue;
        }
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}

void GUI_DrawFont24(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s, uint8_t mode)
{
    uint8_t i, j;
    uint16_t k;
    uint16_t HZnum;
    uint16_t x0 = x;

    HZnum = sizeof(tfont24) / sizeof(typFNT_GB24);
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
        {
            LCD_SetWindows(x, y, x + 24 - 1, y + 24 - 1);
            for (i = 0; i < 24 * 3; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)
                    {
                        if (tfont24[k].Msk[i] & (0x80 >> j))
                            Lcd_WriteData_16Bit(fc);
                        else
                            Lcd_WriteData_16Bit(bc);
                    }
                    else
                    {
                        if (tfont24[k].Msk[i] & (0x80 >> j))
                            GUI_DrawPoint(x, y, fc);
                        x++;
                        if ((x - x0) == 24)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            continue;
        }
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}

void GUI_DrawFont32(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s, uint8_t mode)
{
    uint8_t i, j;
    uint16_t k;
    uint16_t HZnum;
    uint16_t x0 = x;

    HZnum = sizeof(tfont32) / sizeof(typFNT_GB32);
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
        {
            LCD_SetWindows(x, y, x + 32 - 1, y + 32 - 1);
            for (i = 0; i < 32 * 4; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)
                    {
                        if (tfont32[k].Msk[i] & (0x80 >> j))
                            Lcd_WriteData_16Bit(fc);
                        else
                            Lcd_WriteData_16Bit(bc);
                    }
                    else
                    {
                        if (tfont32[k].Msk[i] & (0x80 >> j))
                            GUI_DrawPoint(x, y, fc);
                        x++;
                        if ((x - x0) == 32)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
            continue;
        }
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}

/* ============================================================
 * Show_Str — 显示中英文混排字符串
 * ============================================================ */
void Show_Str(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str, uint8_t size, uint8_t mode)
{
    uint16_t x0 = x;
    uint8_t bHz = 0;

    while (*str != 0)
    {
        if (!bHz)
        {
            if (x > (lcddev.width - size / 2) || y > (lcddev.height - size))
                return;

            if (*str > 0x80) bHz = 1;  /* 汉字 */
            else
            {
                if (*str == 0x0D)       /* 换行符 */
                {
                    y += size;
                    x = x0;
                    str++;
                }
                else
                {
                    if (size > 16)
                    {
                        LCD_ShowChar(x, y, fc, bc, *str, 16, mode);
                        x += 8;
                    }
                    else
                    {
                        LCD_ShowChar(x, y, fc, bc, *str, size, mode);
                        x += size / 2;
                    }
                }
                str++;
            }
        }
        else  /* 汉字 */
        {
            if (x > (lcddev.width - size) || y > (lcddev.height - size))
                return;

            bHz = 0;
            if (size == 32)
                GUI_DrawFont32(x, y, fc, bc, str, mode);
            else if (size == 24)
                GUI_DrawFont24(x, y, fc, bc, str, mode);
            else
                GUI_DrawFont16(x, y, fc, bc, str, mode);

            str += 2;
            x += size;
        }
    }
}

/* ============================================================
 * Gui_StrCenter — 居中显示字符串
 * ============================================================ */
void Gui_StrCenter(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str, uint8_t size, uint8_t mode)
{
    uint16_t len = strlen((const char *)str);
    uint16_t x1 = (lcddev.width - len * 8) / 2;
    (void)x;
    Show_Str(x1, y, fc, bc, str, size, mode);
}

/* ============================================================
 * Gui_Drawbmp16 — 显示 40x40 16位 BMP
 * ============================================================ */
void Gui_Drawbmp16(uint16_t x, uint16_t y, const unsigned char *p)
{
    int i;
    unsigned char picH, picL;

    LCD_SetWindows(x, y, x + 40 - 1, y + 40 - 1);
    for (i = 0; i < 40 * 40; i++)
    {
        picL = *(p + i * 2);
        picH = *(p + i * 2 + 1);
        Lcd_WriteData_16Bit((uint16_t)(picH << 8) | picL);
    }
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
}
