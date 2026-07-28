#include "FreeRTOS.h"
#include "task.h"
#include "usart.h"
#include "bsp_log.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_can.h"
#include "bsp_spi_lcd.h"
#include "bsp_i2c_touch.h"
#include "mod_comm_can.h"
#include "mod_dashboard_data.h"
#include "task_query.h"
#include "task_lcd_demo.h"

void Task_Entry_All(void * pvParameters);
void Heartbeat_Task(void * pvParameters);

/* ============================================================
 * Heartbeat_Task — 心跳任务
 * LED1 每 500ms 翻转 + 每秒 printf 心跳信息
 * ============================================================ */
void Heartbeat_Task(void * pvParameters)
{
    uint32_t tick = 0;
    (void)pvParameters;

    while (1)
    {
        tick++;
        GPIO_ToggleBits(LED1_Port, LED1_Pin);

        if (tick % 2 == 0)
        {
            LOG_I("[HEARTBEAT] tick=%u\r\n", tick / 2);
            Can_Heartbeat();
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ============================================================
 * Task_Entry_All — 一次性初始化入口，创建所有子任务后自删除
 * ============================================================ */
void Task_Entry_All(void * pvParameters)
{
    (void)pvParameters;

    /* ---- BSP 硬件初始化 ---- */
    BSP_LED_Init();
    BSP_KEY_Init();

    Mod_Can_Init();        /* 先创建队列，再初始化硬件使能中断 */
    BSP_CAN_Init();

    BSP_SPI_LCD_Init();    /* SPI5 + ILI9341 */
    BSP_I2C_Touch_Init();  /* I2C1 + FT6336G */
    Dashboard_Data_Init(); /* 在 CAN 子任务启动前创建共享状态和互斥锁 */

    /* ---- 创建 FreeRTOS 任务 ----
     * CAN_TX/RX:  512 字（队列收发 + 硬件调用）
     * CAN_TEST:   256 字（简单轮询）
     * KEY_SCAN:   256 字（GPIO 轮询）
     * HEARTBEAT:  512 字（printf/vsprintf 栈开销大）
     * UART_QUERY: 256 字（串口查询）
     * LCD_DEMO:   512 字（GUI 绘制栈开销大） */
    if (xTaskCreate(Mod_Can_TxTask,   "CAN_TX",     512, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] CAN_TX task create failed!\r\n");
    if (xTaskCreate(Mod_Can_RxTask,   "CAN_RX",     512, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] CAN_RX task create failed!\r\n");
    if (xTaskCreate(CAN_Test_Task,    "CAN_TEST",   256, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] CAN_TEST task create failed!\r\n");
    if (xTaskCreate(prvKeyScanTask,   "KEY_SCAN",   256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] KEY_SCAN task create failed!\r\n");
    if (xTaskCreate(Heartbeat_Task,   "HEARTBEAT",  512, NULL, 1, NULL) != pdPASS)
        LOG_E("[Main] HEARTBEAT task create failed!\r\n");
    if (xTaskCreate(UART_Query_Task,  "UART_QUERY", 256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] UART_QUERY task create failed!\r\n");
    if (xTaskCreate(Task_LCD_Demo,    "LCD_DEMO",  1024, NULL, 3, NULL) != pdPASS)
        LOG_E("[Main] LCD_DEMO task create failed!\r\n");

    vTaskDelete(NULL);
}
