#include "task_lcd_demo.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "demos/widgets/lv_demo_widgets.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * Task_LCD_Demo — LVGL 仪表盘演示任务
 * 初始化 LVGL 图形库并运行控件演示
 * 栈: 1024 字 = 4KB (LVGL 渲染开销)
 * 优先级: 3
 * ============================================================ */
void Task_LCD_Demo(void *pvParameters)
{
    (void)pvParameters;

    LOG_I("[LVGL] Initializing LVGL v%d.%d.%d...\r\n",
          lv_version_major(), lv_version_minor(), lv_version_patch());

    /* LVGL 核心初始化 */
    lv_init();

    /* 显示驱动 (ILI9341 SPI) */
    lv_port_disp_init();
    LOG_I("[LVGL] Display driver initialized\r\n");

    /* 触摸输入 (FT6336G I2C) */
    lv_port_indev_init();
    LOG_I("[LVGL] Input device initialized\r\n");

    /* 启动控件演示 */
    lv_demo_widgets();
    LOG_I("[LVGL] Widgets demo started\r\n");

    /* 主循环: 每 5ms 更新 LVGL tick 并处理渲染 */
    TickType_t last_tick = xTaskGetTickCount();
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        lv_tick_inc((uint32_t)(now - last_tick));
        last_tick = now;

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
