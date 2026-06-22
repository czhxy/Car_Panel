#include "FreeRTOS.h"
#include "task.h"
#include "usart.h"
#include "bsp_log.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "mod_comm_can.h"

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
            printf("[HEARTBEAT] tick=%lu\r\n", tick);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ============================================================
 * Task_Entry_All — 一次性的初始化入口
 * 创建所有子任务后自身返回
 * ============================================================ */
void Task_Entry_All(void * pvParameters)
{
    (void)pvParameters;

    BSP_LED_Init();
    BSP_KEY_Init();

    /* 提前创建 CAN 队列，避免 RX 任务在队列就绪前启动 */
    Mod_Can_Init();

    /* 栈大小经过评估：
     * CAN_TX/RX: 512 字 = 2KB（队列收发 + 硬件调用）
     * CAN_TEST/KEY_SCAN: 256 字 = 1KB（简单轮询）
     * HEARTBEAT: 512 字 = 2KB（printf/vsprintf 栈开销大） */
    if (xTaskCreate(Mod_Can_TxTask, "CAN_TX", 512, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] CAN_TX task create failed!\r\n");
    if (xTaskCreate(Mod_Can_RxTask, "CAN_RX", 512, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] CAN_RX task create failed!\r\n");
    if (xTaskCreate(CAN_Test_Task,  "CAN_TEST", 256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] CAN_TEST task create failed!\r\n");
    if (xTaskCreate(KEYTask, "KEY_SCAN", 256, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] KEY_SCAN task create failed!\r\n");
    if (xTaskCreate(Heartbeat_Task, "HEARTBEAT", 512, NULL, 2, NULL) != pdPASS)
        LOG_E("[Main] HEARTBEAT task create failed!\r\n");

    vTaskDelete(NULL);
}
