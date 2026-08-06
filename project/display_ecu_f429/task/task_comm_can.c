/* task_comm_can.c — CAN 发送/接收任务
 *
 * 任务函数原位于 mod/mod_comm_can.c，按「任务放 task 层、机制放 mod 层」拆分：
 *   - Task_CanTx    : 发送任务（统一消费 TX 队列 + 周期推送电机控制帧）
 *   - Task_CanRx    : 接收任务（从 RX 队列取帧 → ModCommCan_OnRxFrame 解析）
 *   - Task_CanTest  : 测试任务（KEY1 触发发送一帧测试报文，预留）
 *
 * 任务调用的机制接口（ModCommCan_Tx / Mod_Can_RxDequeue / ModCommCan_OnRxFrame
 * / Mod_Can_TxTest / CanProtocol_WheelCtlSend）均位于 mod 层。
 */

#include "task_comm_can.h"
#include "mod_comm_can.h"
#include "mod_can_protocol.h"
#include "bsp_key.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================
 * Task_CanTx — CAN 发送任务
 * ① 统一消费 TX 队列发送 → ② 周期推送电机控制帧 → ③ 让出 CPU
 * ============================================================ */
void Task_CanTx(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        ModCommCan_Tx();               /* ① 数据发送：统一消费 TX 队列 */
        CanProtocol_WheelCtlSend();    /* ② 数据推送：电机控制帧（10ms 限频）*/
        vTaskDelay(pdMS_TO_TICKS(1));  /* ③ 让出 CPU */
    }
}

/* ============================================================
 * Task_CanRx — CAN 接收任务
 * 从队列取帧 → 调 ModCommCan_OnRxFrame → 批量处理
 * ============================================================ */
void Task_CanRx(void *pvParameters)
{
    CanRxMsg rx_msg;

    (void)pvParameters;

    while (1) {
        if (Mod_Can_RxDequeue(&rx_msg, portMAX_DELAY)) {
            ModCommCan_OnRxFrame(&rx_msg);

            /* 本轮继续处理队列中剩余的消息 */
            while (Mod_Can_RxDequeue(&rx_msg, 0)) {
                ModCommCan_OnRxFrame(&rx_msg);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================
 * Task_CanTest — CAN 测试任务（预留）
 * 按下 KEY1 后发送一帧测试报文
 * ============================================================ */
void Task_CanTest(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        if (xSemaphoreTake(xKey1Sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            Mod_Can_TxTest();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
