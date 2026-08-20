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
#include "mod_comm_uart.h"
#include "mod_ui.h"
#include "mod_query.h"
#include "task_ui.h"
#include "task_comm_can.h"
#include "task_comm_uart.h"
#include "boot_wdg.h"
#include "ota_params.h"

void Task_Entry_All(void * pvParameters);
void Heartbeat_Task(void * pvParameters);

/* ============================================================
 * App_Ota_Confirm_Active — 启动成功后确认当前槽运行正常
 * 把 OTA 参数区从 COMPLETE（待确认）收敛为 IDLE 并清零启动计数，
 * 结束 bootloader 的回滚观察窗口（boot_count 不再累计）。
 * 仅改动 ota_state / boot_count，不触碰 active_partition
 * （分区选择由 bootloader 唯一负责）。
 * ============================================================ */
static void App_Ota_Confirm_Active(void)
{
    ota_param_t param;

    ota_params_load(&param);
    if (param.magic != OTA_MAGIC) {
        return;   /* 参数区无效（如直接烧录 App 运行），无需确认 */
    }

    if (param.ota_state == OTA_STATE_COMPLETE) {
        param.ota_state = OTA_STATE_IDLE;
        param.boot_count = 0;
        if (ota_params_save(&param) == 0) {
            LOG_I("[OTA] Active slot confirmed, state COMPLETE -> IDLE.\r\n");
        } else {
            LOG_W("[OTA] Confirm save failed!\r\n");
        }
    }
}

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
        wdg_feed();   /* 喂 IWDG：bootloader 跳转时已启动，App 常驻喂狗防运行期复位 */
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

    /* Mod_Uart_Init 已提前到 main.c（UART_Init 之后）调用，此处不再重复创建队列 */
    Mod_Query_Init();      /* 查询协议业务初始化（确保 mod_query 被链接） */

    BSP_SPI_LCD_Init();    /* SPI5 + ILI9341 */
    BSP_I2C_Touch_Init();  /* I2C1 + FT6336G */
    Dashboard_Data_Init(); /* 在 CAN 子任务启动前创建共享状态和互斥锁 */

    /* ---- 创建 FreeRTOS 任务 ----
     * CAN_TX/RX:  512 字（队列收发 + 硬件调用）
     * CAN_TEST:   256 字（简单轮询）
     * KEY_SCAN:   256 字（GPIO 轮询）
     * HEARTBEAT:  512 字（printf/vsprintf 栈开销大）
     * UART_TX:    256 字（TX 队列消费 + 串口发送）
     * UART_RX:    256 字（字节队列 + 拼包 + 业务回调）
     * UI:        1024 字（LVGL 渲染开销大） */
    if (xTaskCreate(Task_CanTx,       "CAN_TX",     512, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] CAN_TX task create failed!\r\n");
    if (xTaskCreate(Task_CanRx,       "CAN_RX",     512, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] CAN_RX task create failed!\r\n");
    if (xTaskCreate(prvKeyScanTask,   "KEY_SCAN",   256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] KEY_SCAN task create failed!\r\n");
    if (xTaskCreate(Heartbeat_Task,   "HEARTBEAT",  512, NULL, 1, NULL) != pdPASS)
        LOG_E("[Main] HEARTBEAT task create failed!\r\n");
    if (xTaskCreate(Task_UartTx,      "UART_TX",    256, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] UART_TX task create failed!\r\n");
    if (xTaskCreate(Task_UartRx,      "UART_RX",    256, NULL, 4, NULL) != pdPASS)
        LOG_E("[Main] UART_RX task create failed!\r\n");
    if (xTaskCreate(Task_UI,          "UI",       1024, NULL, 3, NULL) != pdPASS)
        LOG_E("[Main] UI task create failed!\r\n");

    /* 所有子系统初始化 + 任务创建完成 = 启动成功，向 bootloader 确认存活 */
    App_Ota_Confirm_Active();

    vTaskDelete(NULL);
}
