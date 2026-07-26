#include "mod_test.h"
#include "mod_gui.h"
#include "mod_test_pic.h"
#include "bsp_spi_lcd.h"
#include "bsp_i2c_touch.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint16_t ColorTab[5] = {RED, GREEN, BLUE, YELLOW, BRED};

/* ============================================================
 * DrawTestPage — 统一的测试页框架
 * ============================================================ */
void DrawTestPage(uint8_t *str)
{
    LCD_Clear(WHITE);
    LCD_Fill(0, 0, lcddev.width, 20, BLUE);
    LCD_Fill(0, lcddev.height - 20, lcddev.width, lcddev.height, BLUE);
    POINT_COLOR = WHITE;
    Gui_StrCenter(0, 2, WHITE, BLUE, str, 16, 1);
    Gui_StrCenter(0, lcddev.height - 18, WHITE, BLUE, "http://www.lcdwiki.com", 16, 1);
}

/* ============================================================
 * Test_Color — 纯色填充测试
 * ============================================================ */
void Test_Color(void)
{
    LCD_Fill(0, 0, lcddev.width, lcddev.height, WHITE);
    Show_Str(20, 30, BLACK, YELLOW, "WHITE", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
    LCD_Fill(0, 0, lcddev.width, lcddev.height, BLACK);
    Show_Str(20, 30, WHITE, YELLOW, "BLACK", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
    LCD_Fill(0, 0, lcddev.width, lcddev.height, RED);
    Show_Str(20, 30, BLUE, YELLOW, "RED ", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
    LCD_Fill(0, 0, lcddev.width, lcddev.height, GREEN);
    Show_Str(20, 30, BLUE, YELLOW, "GREEN ", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
    LCD_Fill(0, 0, lcddev.width, lcddev.height, BLUE);
    Show_Str(20, 30, RED, YELLOW, "BLUE ", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
    LCD_Fill(0, 0, lcddev.width, lcddev.height, GRAY);
    Show_Str(20, 30, MAGENTA, YELLOW, "GRAY", 16, 1); vTaskDelay(pdMS_TO_TICKS(800));
}

/* ============================================================
 * Test_FillRec — 矩形测试
 * ============================================================ */
void Test_FillRec(void)
{
    uint8_t i = 0;
    DrawTestPage("Test: Rect");
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_DrawRectangle(lcddev.width / 2 - 80 + (i * 15),
                          lcddev.height / 2 - 80 + (i * 15),
                          lcddev.width / 2 - 80 + (i * 15) + 60,
                          lcddev.height / 2 - 80 + (i * 15) + 60);
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_DrawFillRectangle(lcddev.width / 2 - 80 + (i * 15),
                              lcddev.height / 2 - 80 + (i * 15),
                              lcddev.width / 2 - 80 + (i * 15) + 60,
                              lcddev.height / 2 - 80 + (i * 15) + 60);
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
}

/* ============================================================
 * Test_Circle — 圆形测试
 * ============================================================ */
void Test_Circle(void)
{
    uint8_t i = 0;
    DrawTestPage("Test: Circle");
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
        gui_circle(lcddev.width / 2 - 80 + (i * 25), lcddev.height / 2 - 50 + (i * 25), ColorTab[i], 30, 0);
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
        gui_circle(lcddev.width / 2 - 80 + (i * 25), lcddev.height / 2 - 50 + (i * 25), ColorTab[i], 30, 1);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

/* ============================================================
 * Test_Triangle — 三角形测试
 * ============================================================ */
void Test_Triangle(void)
{
    uint8_t i = 0;
    DrawTestPage("Test: Triangle");
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        Draw_Triangel(lcddev.width / 2 - 80 + (i * 20), lcddev.height / 2 - 20 + (i * 15),
                      lcddev.width / 2 - 50 - 1 + (i * 20), lcddev.height / 2 - 20 - 52 - 1 + (i * 15),
                      lcddev.width / 2 - 20 - 1 + (i * 20), lcddev.height / 2 - 20 + (i * 15));
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    LCD_Fill(0, 20, lcddev.width, lcddev.height - 20, WHITE);
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        Fill_Triangel(lcddev.width / 2 - 80 + (i * 20), lcddev.height / 2 - 20 + (i * 15),
                      lcddev.width / 2 - 50 - 1 + (i * 20), lcddev.height / 2 - 20 - 52 - 1 + (i * 15),
                      lcddev.width / 2 - 20 - 1 + (i * 20), lcddev.height / 2 - 20 + (i * 15));
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
}

/* ============================================================
 * English_Font_test — 英文字体测试
 * ============================================================ */
void English_Font_test(void)
{
    DrawTestPage("Test: English Font");
    Show_Str(10, 30, BLUE, YELLOW, "6X12:abcdefghijklmnopqrstuvwxyz0123456789", 12, 0);
    Show_Str(10, 45, BLUE, YELLOW, "6X12:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 12, 1);
    Show_Str(10, 60, BLUE, YELLOW, "6X12:~!@#$%^&*()_+{}:<>?/|-+.", 12, 0);
    Show_Str(10, 80, BLUE, YELLOW, "8X16:abcdefghijklmnopqrstuvwxyz0123456789", 16, 0);
    Show_Str(10, 100, BLUE, YELLOW, "8X16:ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 16, 1);
    Show_Str(10, 120, BLUE, YELLOW, "8X16:~!@#$%^&*()_+{}:<>?/|-+.", 16, 0);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

/* ============================================================
 * Chinese_Font_test — 中文字体测试
 * ============================================================ */
void Chinese_Font_test(void)
{
    DrawTestPage("Test: Font Test");
    Show_Str(10, 30, BLUE, YELLOW, "16X16: QDtech Welcome!", 16, 0);
    Show_Str(10, 50, BLUE, YELLOW, "16X16: Welcome QDtech", 16, 0);
    Show_Str(10, 70, BLUE, YELLOW, "24X24: Font 24 Test", 24, 1);
    Show_Str(10, 100, BLUE, YELLOW, "32X32: Font 32", 32, 1);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

/* ============================================================
 * Pic_test — 图片显示测试
 * ============================================================ */
void Pic_test(void)
{
    DrawTestPage("Test: Picture");
    Gui_Drawbmp16(30, 30, gImage_qq);
    Show_Str(30 + 12, 75, BLUE, YELLOW, "QQ", 16, 1);
    Gui_Drawbmp16(90, 30, gImage_qq);
    Show_Str(90 + 12, 75, BLUE, YELLOW, "QQ", 16, 1);
    Gui_Drawbmp16(150, 30, gImage_qq);
    Show_Str(150 + 12, 75, BLUE, YELLOW, "QQ", 16, 1);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

/* ============================================================
 * Test_Dynamic_Num — 动态数值测试
 * ============================================================ */
void Test_Dynamic_Num(void)
{
    uint8_t i;
    DrawTestPage("Test: Dynamic Number");
    POINT_COLOR = BLUE;
    srand(123456);
    LCD_ShowString(15, 50, 16, " HCHO:           ug/m3", 1);
    LCD_ShowString(15, 70, 16, "  CO2:           ppm", 1);
    LCD_ShowString(15, 90, 16, " TVOC:           ug/m3", 1);
    LCD_ShowString(15, 110, 16, "PM2.5:           ug/m3", 1);
    LCD_ShowString(15, 130, 16, " PM10:           ug/m3", 1);
    LCD_ShowString(15, 150, 16, "  TEP:           C", 1);
    LCD_ShowString(15, 170, 16, "  HUM:           %", 1);
    POINT_COLOR = RED;
    for (i = 0; i < 15; i++)
    {
        LCD_ShowNum(100, 50, rand() % 10000, 5, 16);
        LCD_ShowNum(100, 70, rand() % 10000, 5, 16);
        LCD_ShowNum(100, 90, rand() % 10000, 5, 16);
        LCD_ShowNum(100, 110, rand() % 10000, 5, 16);
        LCD_ShowNum(100, 130, rand() % 10000, 5, 16);
        LCD_ShowNum(100, 150, rand() % 50, 5, 16);
        LCD_ShowNum(100, 170, rand() % 100, 5, 16);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ============================================================
 * Touch_Test — 触摸坐标测试
 * 显示触摸点坐标，持续 10 秒
 * ============================================================ */
void Touch_Test(void)
{
    uint32_t start_tick = xTaskGetTickCount();
    uint8_t touched = 0;

    DrawTestPage("Touch Test");
    Show_Str(20, 40, BLUE, YELLOW, "Touch the screen to test", 16, 0);
    Show_Str(20, 70, BLUE, YELLOW, "X: ----  Y: ----", 16, 1);

    POINT_COLOR = RED;

    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(8000))
    {
        if (tp_dev.scan && tp_dev.scan())
        {
            if (tp_dev.sta & (1 << 0))
            {
                touched = 1;
                LCD_ShowNum(60, 70, tp_dev.x[0], 4, 16);
                LCD_ShowNum(180, 70, tp_dev.y[0], 4, 16);

                /* 绘制触摸轨迹点 */
                if (tp_dev.x[0] < lcddev.width && tp_dev.y[0] < lcddev.height)
                {
                    if (tp_dev.y[0] > 100)
                        gui_circle(tp_dev.x[0], tp_dev.y[0], RED, 3, 1);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    POINT_COLOR = BRED;
    if (touched)
        Gui_StrCenter(0, 120, BRED, WHITE, "Touch OK!", 16, 1);
    else
        Gui_StrCenter(0, 120, BRED, WHITE, "No touch detected", 16, 1);

    LOG_I("[TOUCH] Test result: %s\r\n", touched ? "OK" : "NO TOUCH");
    vTaskDelay(pdMS_TO_TICKS(1500));
}
