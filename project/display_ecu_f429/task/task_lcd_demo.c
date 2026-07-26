#include "task_lcd_demo.h"
#include "mod_gui.h"
#include "mod_test.h"
#include "bsp_spi_lcd.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * Task_LCD_Demo — LCD 综合演示任务
 * 循环运行各项测试，每项间隔 2 秒
 * 栈：512 字 = 2KB
 * 优先级：3
 * ============================================================ */
void Task_LCD_Demo(void *pvParameters)
{
    (void)pvParameters;

    LOG_I("[LCD_DEMO] Task started\r\n");

    while (1)
    {
//        /* 1. 主界面 */
//        DrawTestPage("QDtech Test");
//        Gui_StrCenter(0, 30, RED, BLUE, "QDtech", 16, 1);
//        Gui_StrCenter(0, 60, RED, BLUE, "Test Program", 16, 1);
//        Gui_StrCenter(0, 90, BRED, WHITE, "2.8\" IPS ILI9341 240X320", 16, 1);
//        Gui_StrCenter(0, 120, BLUE, WHITE, "SPI5 + ILI9341 SPL", 16, 1);
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 2. 纯色填充 */
//        Test_Color();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 3. 矩形测试 */
//        Test_FillRec();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 4. 圆形测试 */
//        Test_Circle();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 5. 三角形测试 */
//        Test_Triangle();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 6. 英文字体测试 */
//        English_Font_test();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 7. 中文字体测试 */
//        Chinese_Font_test();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 8. 图片显示 */
//        Pic_test();
//        vTaskDelay(pdMS_TO_TICKS(2000));

//        /* 9. 动态数值测试 */
//        Test_Dynamic_Num();
//        vTaskDelay(pdMS_TO_TICKS(2000));

        /* 10. 触摸坐标测试 */
        Touch_Test();
//        vTaskDelay(pdMS_TO_TICKS(2000));

        LOG_I("[LCD_DEMO] Loop completed, restarting...\r\n");
    }
}
