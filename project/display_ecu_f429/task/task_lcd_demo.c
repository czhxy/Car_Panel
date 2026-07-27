#include "task_lcd_demo.h"
#include "task_dashboard_ui.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * Task_LCD_Demo — LVGL 仪表盘 UI 任务
 * 初始化 LVGL 图形库并运行仪表盘界面
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

    /* 构建仪表盘 UI (替换原 lv_demo_widgets) */
    Dashboard_UI_Init(lv_scr_act());
    LOG_I("[LVGL] Dashboard UI started\r\n");

    /* 主循环: 每 5ms 更新 LVGL tick + 处理渲染
     * 每 25ms 调用 Dashboard_Update 刷新动态元素 */
    TickType_t last_tick = xTaskGetTickCount();
    uint32_t loop_cnt = 0;
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        lv_tick_inc((uint32_t)(now - last_tick));
        last_tick = now;

        lv_timer_handler();

        /* 每 5 次循环 (25ms) 更新仪表盘数据 */
        if ((loop_cnt % 5) == 0) {
            Dashboard_Update();
        }
        loop_cnt++;

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
