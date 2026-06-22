#include "mod_comm_can.h"
#include "bsp_can.h"
#include "bsp_key.h"
#include "bsp_log.h"
#include <string.h>
#include "task.h"

/* ============================================================
 * 弱符号兼容宏
 * MDK: __weak / GCC: __attribute__((weak))
 * ============================================================ */
#if defined(__GNUC__) && !defined(__CC_ARM)
  #define WEAK __attribute__((weak))
#else
  #define WEAK __weak
#endif

/* ---- 静态变量 ---- */
static QueueHandle_t CanTxQueue = NULL;
static QueueHandle_t CanRxQueue = NULL;

static struct {
    uint8_t tx_err_count;
    uint8_t rx_err_count;
} event_err_count;

/* ---- 统计变量（非静态，供外部只读访问） ---- */
const uint8_t *ModCan_TxErrCount  = &event_err_count.tx_err_count;
const uint8_t *ModCan_RxErrCount  = &event_err_count.rx_err_count;
const uint8_t *ModCan_RxErrCount_ = &event_err_count.rx_err_count;

/* ============================================================
 * Mod_Can_Init — 创建 TX/RX FreeRTOS 队列
 * ============================================================ */
void Mod_Can_Init(void)
{
    CanTxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanTxMsg));
    CanRxQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CanRxMsg));
}

/* ============================================================
 * Mod_Can_TxEvent — 非阻塞入 TX 队列
 * ============================================================ */
bool Mod_Can_TxEvent(CanTxMsg tx_message)
{
    if (CanTxQueue == NULL) {
        return false;
    }
    if (xQueueSend(CanTxQueue, &tx_message, 0) == pdPASS) {
        return true;
    }
    event_err_count.tx_err_count++;
    return false;
}

/* ============================================================
 * Mod_Can_RxIRQHandler — FIFO0 中断读取，入接收队列
 * 在 CAN1_RX0_IRQHandler 中调用
 * ============================================================ */
void Mod_Can_RxIRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CanRxMsg rx_msg;

    while (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET) {
        CAN_Receive(CAN1, CAN_FIFO0, &rx_msg);

        if (CanRxQueue != NULL) {
            xQueueSendFromISR(CanRxQueue, &rx_msg, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================
 * ModCommCan_PrintRxFrame — 解析并打印接收帧详情
 * ============================================================ */
void ModCommCan_PrintRxFrame(const CanRxMsg *rx_msg)
{
    uint8_t i;

    if (rx_msg->IDE == CAN_ID_EXT) {
        uint32_t id = rx_msg->ExtId;
        LOG_D("[RX] ExtID=0x%08X src=%d dst=%d type=%d mode=0x%03X func=%d DLC=%d data:",
                   id,
                   CAN_ID_GET_SRC(id),
                   CAN_ID_GET_DST(id),
                   CAN_ID_GET_FTYPE(id),
                   CAN_ID_GET_MODE(id),
                   CAN_ID_GET_FUNC(id),
                   rx_msg->DLC);
    } else {
        LOG_D("[RX] StdID=0x%03X DLC=%d data:", rx_msg->StdId, rx_msg->DLC);
    }

    for (i = 0; i < rx_msg->DLC; i++) {
        LOG_D(" %02X", rx_msg->Data[i]);
    }
    LOG_D("\r\n");
}

/* ============================================================
 * ModCommCan_OnRxFrame — 弱符号接收回调
 * 默认行为：打印帧内容；应用层可通过重写接管
 * ============================================================ */
WEAK void ModCommCan_OnRxFrame(const CanRxMsg *rx_msg)
{
    ModCommCan_PrintRxFrame(rx_msg);
}

/* ============================================================
 * Mod_Can_TxTask — 发送任务
 * 开头调用 BSP_CAN_Init 进行硬件初始化（需在调度器启动后）
 * ============================================================ */
void Mod_Can_TxTask(void *pvParameters)
{
    CanTxMsg tx_pack;

    (void)pvParameters;

    /* 硬件初始化（必须在调度器启动后执行，因为包含 vTaskDelay）
     * 队列已由 task_entry 提前创建 */
    BSP_CAN_Init();
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {
        if (xQueueReceive(CanTxQueue, &tx_pack, portMAX_DELAY) == pdPASS) {
            uint8_t mbox = CAN_Transmit(CAN1, &tx_pack);
            if (mbox == CAN_TxStatus_NoMailBox) {
                /* 邮箱满，回灌队首并让出 CPU */
                xQueueSendToFront(CanTxQueue, &tx_pack, 0);
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

/* ============================================================
 * Mod_Can_RxTask — 接收任务
 * 从队列取帧 → 调 ModCommCan_OnRxFrame → 批量处理
 * ============================================================ */
void Mod_Can_RxTask(void *pvParameters)
{
    CanRxMsg rx_msg;

    (void)pvParameters;

    while (1) {
        if (xQueueReceive(CanRxQueue, &rx_msg, portMAX_DELAY) == pdPASS) {
            ModCommCan_OnRxFrame(&rx_msg);

            /* 本轮继续处理队列中剩余的消息 */
            while (xQueueReceive(CanRxQueue, &rx_msg, 0) == pdPASS) {
                ModCommCan_OnRxFrame(&rx_msg);
            }
        }
    }
}

/* ============================================================
 * Mod_Can_TxTest — 构造并发送一帧测试报文
 * 8 字节递增数据，使用 CAN_TX_ID 扩展帧
 * ============================================================ */
void Mod_Can_TxTest(void)
{
    static uint8_t seq = 0;
    CanTxMsg tx_msg;
    memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.ExtId  = CAN_TX_ID;
    tx_msg.IDE    = CAN_ID_EXT;
    tx_msg.RTR    = CAN_RTR_DATA;
    tx_msg.DLC    = 8;

    tx_msg.Data[0] = seq++;
    tx_msg.Data[1] = seq++;
    tx_msg.Data[2] = seq++;
    tx_msg.Data[3] = seq++;
    tx_msg.Data[4] = seq++;
    tx_msg.Data[5] = seq++;
    tx_msg.Data[6] = seq++;
    tx_msg.Data[7] = seq++;

    Mod_Can_TxEvent(tx_msg);
}

/* ============================================================
 * CAN_Test_Task — 测试任务
 * 独立轮询 KEY1，不消费其他任务的信号量
 * 按下 KEY1 后发送一帧测试报文
 * ============================================================ */
void CAN_Test_Task(void *pvParameters)
{
    uint8_t debounce = 0;
    (void)pvParameters;

    while (1) {
        if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET) {
            if (debounce == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));
                if (GPIO_ReadInputDataBit(KEY1_Port, KEY1_Pin) == Bit_RESET) {
                    debounce = 1;
                    Mod_Can_TxTest();
                }
            }
        } else {
            debounce = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
